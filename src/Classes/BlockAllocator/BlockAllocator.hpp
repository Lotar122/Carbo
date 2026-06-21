#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <new>

//Claude

namespace Carbo
{
    // Slab-based fixed-size object pool.
    //
    // Complexity vs. original:
    //   allocate()      O(numSlabs) worst case -> O(1) amortized
    //   free()          O(numSlabs)             -> O(log numSlabs)
    //   ~Slab()         O(used * freeCount)      -> O(used)
    //   per-slab memory O(capacity * sizeof(T*)) -> O(capacity / 64) (bitset)
    //
    // Not thread-safe. If T objects here are touched from multiple threads
    // (e.g. MsQuic worker-thread callbacks), wrap allocate()/free() in a
    // mutex or give each worker thread its own BlockAllocator instance.
    template<typename T>
    class BlockAllocator
    {
        struct Slab
        {
            T* data;
            size_t                 capacity;
            size_t                 used = 0;            // bump cursor, monotonic
            std::vector<uint64_t>  liveBits;             // 1 bit per slot, set = constructed

            explicit Slab(size_t cap)
                : data(static_cast<T*>(
                    ::operator new(sizeof(T)* cap, std::align_val_t{ alignof(T) })))
                , capacity(cap)
                , liveBits((cap + 63) / 64, 0)
            {
            }

            Slab(const Slab&) = delete;
            Slab& operator=(const Slab&) = delete;

            ~Slab()
            {
                // Single pass over live bits - no more O(n*m) freelist scan.
                for (size_t i = 0; i < used; ++i)
                {
                    if (isLive(i))
                        (data + i)->~T();
                }
                ::operator delete(data, std::align_val_t{ alignof(T) });
            }

            bool isLive(size_t idx) const noexcept
            {
                return (liveBits[idx >> 6] >> (idx & 63)) & 1ull;
            }
            void setLive(size_t idx) noexcept
            {
                liveBits[idx >> 6] |= (uint64_t{ 1 } << (idx & 63));
            }
            void clearLive(size_t idx) noexcept
            {
                liveBits[idx >> 6] &= ~(uint64_t{ 1 } << (idx & 63));
            }

            // Returns a fresh slot from the bump region, or nullptr if full.
            T* tryBump() noexcept
            {
                if (used >= capacity)
                    return nullptr;
                return data + used++;
            }
        };

        std::vector<std::unique_ptr<Slab>>   slabs;       // owns slabs (exception-safe)
        std::vector<std::pair<T*, Slab*>>    freeList;     // {slot, owning slab}, LIFO reuse
        std::vector<std::pair<T*, Slab*>>    slabRanges;   // sorted by base ptr, for free()
        size_t                               slabSize;
        Slab* currentSlab = nullptr;

        void registerSlab(Slab* s)
        {
            // Keep slabRanges sorted by base address so free() can binary search.
            auto it = std::upper_bound(
                slabRanges.begin(), slabRanges.end(), s->data,
                [](T* p, const std::pair<T*, Slab*>& e) { return p < e.first; });
            slabRanges.insert(it, { s->data, s });
        }

        Slab* growNewSlab()
        {
            auto owned = std::make_unique<Slab>(slabSize);
            Slab* raw = owned.get();
            slabs.push_back(std::move(owned));
            registerSlab(raw);
            currentSlab = raw;
            return raw;
        }

        Slab* findSlab(T* ptr) const
        {
            // First slab whose base address is > ptr, then step back one.
            auto it = std::upper_bound(
                slabRanges.begin(), slabRanges.end(), ptr,
                [](T* p, const std::pair<T*, Slab*>& e) { return p < e.first; });
            if (it == slabRanges.begin())
                return nullptr;
            --it;
            Slab* s = it->second;
            if (ptr >= s->data && ptr < s->data + s->capacity)
                return s;
            return nullptr;
        }

    public:
        explicit BlockAllocator(size_t _slabSize = 1024)
            : slabSize(_slabSize ? _slabSize : 1)
        {
            growNewSlab();
        }

        // Owns raw heap memory directly (via operator new) - copying would
        // double-free on destruction, so disallow it. Moves are fine.
        BlockAllocator(const BlockAllocator&) = delete;
        BlockAllocator& operator=(const BlockAllocator&) = delete;
        BlockAllocator(BlockAllocator&&) noexcept = default;
        BlockAllocator& operator=(BlockAllocator&&) noexcept = default;
        ~BlockAllocator() = default; // unique_ptr<Slab> cleans everything up

        template<typename... Args>
        T* allocate(Args&&... args)
        {
            T* slot;
            Slab* owner;

            if (!freeList.empty())
            {
                slot = freeList.back().first;
                owner = freeList.back().second;
                freeList.pop_back();
            }
            else
            {
                slot = currentSlab->tryBump();
                if (!slot)
                {
                    currentSlab = growNewSlab();
                    slot = currentSlab->tryBump();
                }
                owner = currentSlab;
            }

            try
            {
                T* obj = new (slot) T(std::forward<Args>(args)...);
                owner->setLive(static_cast<size_t>(slot - owner->data));
                return obj;
            }
            catch (...)
            {
                // Construction failed: slot was never marked live, just
                // hand it back to the free list so capacity isn't lost.
                freeList.push_back({ slot, owner });
                throw;
            }
        }

        void free(T* ptr)
        {
            if (!ptr)
                return;

            Slab* owner = findSlab(ptr);
            if (!owner)
                return; // not from this allocator - ignore (assert in debug if you prefer)

            size_t idx = static_cast<size_t>(ptr - owner->data);
            if (!owner->isLive(idx))
                return; // guards against double-free

            ptr->~T();
            owner->clearLive(idx);
            freeList.push_back({ ptr, owner });
        }
    };
}