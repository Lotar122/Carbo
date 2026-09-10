#pragma once

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <cstdlib>
#include <cassert>

#ifdef _WIN32 
    #include <malloc.h>
#endif

namespace Carbo
{
    //There's no boundcheck apart from an assert.
    template<size_t align = 4>
    class AtomicBumpAllocator
    {
        struct alignas(64) AtomicWrapper
        {
            std::atomic<std::byte*> atomic;
        };

        AtomicWrapper atomicWrapper;

        std::atomic<std::byte*>& currentOffset = atomicWrapper.atomic;

        size_t size = 0;
        std::byte* memory = nullptr;
    public:
        explicit AtomicBumpAllocator(size_t _size) : size(_size)
        {
            static_assert(align != 0 && (align & (align - 1)) == 0);
            #ifdef _WIN32
                memory = reinterpret_cast<std::byte*>(_aligned_malloc(size, align));
            #else
                memory = reinterpret_cast<std::byte*>(std::aligned_alloc(align, size));
            #endif
            currentOffset.store(memory, std::memory_order_seq_cst);
        }

        ~AtomicBumpAllocator()
        {
            #ifdef _WIN32
                _aligned_free(memory);
            #else
                std::free(memory);
            #endif
        }

        template<typename T>
        T* allocate()
        {
            static_assert(align >= alignof(T),
                      "Allocator alignment is insufficient for T");

            constexpr std::size_t allocSize =
                (sizeof(T) + align - 1) & ~(align - 1);

            std::byte* ptr = currentOffset.fetch_add(
                allocSize,
                std::memory_order_relaxed
            );

            assert(ptr + sizeof(T) <= memory + size);

            return reinterpret_cast<T*>(ptr);
        }

        void reset()
        {
            currentOffset.store(memory, std::memory_order_seq_cst);
        }

        size_t _size()
        {
            return size;
        }

        void* _data()
        {
            return memory;
        }

        //USE WITH CAUTION, this is used to get the member at an index ONLY IF THE ALLOCATOR IS AN ARRAY WITH MEMBERS OF THE SAME TYPE
        template<typename T>
        T& at(size_t index)
        {
            return *(reinterpret_cast<T*>(memory) + index);
        }
    };
}