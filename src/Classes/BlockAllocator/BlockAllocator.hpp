#pragma once

#include <vector>
#include <cstddef>
#include <new>
#include <limits>

namespace Carbo
{
    template<typename T>
    class BlockAllocator
    {
        struct Slab
        {
            T* data = nullptr;
            size_t capacity = 0;
            size_t used = 0;

            T** freeList = nullptr;
            size_t freeCount = 0;
            size_t freeCap = 0;

            Slab(size_t cap)
                : capacity(cap)
            {
                data = static_cast<T*>(
                    ::operator new(sizeof(T) * cap, std::align_val_t{ alignof(T) })
                    );

                freeCap = cap;
                freeList = new T * [freeCap];
            }

            ~Slab()
            {
                // destroy live objects (not in freelist)
                for (size_t i = 0; i < used; ++i)
                {
                    T* ptr = data + i;

                    // check if pointer is in freelist
                    bool isFree = false;
                    for (size_t j = 0; j < freeCount; ++j)
                    {
                        if (freeList[j] == ptr)
                        {
                            isFree = true;
                            break;
                        }
                    }

                    if (!isFree)
                        ptr->~T();
                }

                delete[] freeList;
                ::operator delete(data, std::align_val_t{ alignof(T) });
            }

            T* allocateSlot()
            {
                if (freeCount > 0)
                {
                    return freeList[--freeCount];
                }

                if (used < capacity)
                {
                    return data + used++;
                }

                return nullptr;
            }

            void freeSlot(T* ptr)
            {
                freeList[freeCount++] = ptr;
            }
        };

        std::vector<Slab*> slabs;
        size_t slabSize;

    public:
        explicit BlockAllocator(size_t _slabSize = 1024)
            : slabSize(_slabSize)
        {
            slabs.push_back(new Slab(slabSize));
        }

        ~BlockAllocator()
        {
            for (auto* s : slabs)
                delete s;
        }

        template<typename... Args>
        T* allocate(Args&&... args)
        {
            for (auto* slab : slabs)
            {
                if (T* slot = slab->allocateSlot())
                {
                    return new (slot) T(std::forward<Args>(args)...);
                }
            }

            // grow new slab
            Slab* newSlab = new Slab(slabSize);
            slabs.push_back(newSlab);

            T* slot = newSlab->allocateSlot();
            return new (slot) T(std::forward<Args>(args)...);
        }

        void free(T* ptr)
        {
            if (!ptr)
                return;

            // find slab containing pointer
            for (auto* slab : slabs)
            {
                if (ptr >= slab->data &&
                    ptr < slab->data + slab->capacity)
                {
                    ptr->~T();
                    slab->freeSlot(ptr);
                    return;
                }
            }

            // invalid pointer -> ignore or assert in debug builds
        }
    };
}