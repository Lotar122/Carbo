#pragma once

#include <cstddef>
#include <cstdint>
#if defined(_MSC_VER)
    #include <intrin.h>
#endif

namespace Carbo {

    enum class PrefetchHint : int
    {
        #if defined(__GNUC__) || defined(__clang__)
            nt,
            t2,
            t1,
            t0
        #elif defined(_MSC_VER)
            nt = _MM_HINT_NTA,
            t2 = _MM_HINT_T2,
            t1 = _MM_HINT_T1,
            t0 = _MM_HINT_T0
        #endif

    };

    #if defined(__GNUC__) || defined(__clang__)

        // GCC/Clang on x86, ARM, RISC-V, etc.
        template<PrefetchHint hint = PrefetchHint::t2>
        inline void prefetch(const void* address) noexcept
        {
            __builtin_prefetch(address, 0, static_cast<const int>(hint));
        }

    #elif defined(_MSC_VER)

        #if defined(_M_X64) || defined(_M_IX86)

            template<PrefetchHint hint = PrefetchHint::t2>
            inline void prefetch(const void* address) noexcept
            {
                _mm_prefetch(
                    static_cast<const char*>(address),
                    static_cast<const int>(hint)
                );
            }

        #elif defined(_M_ARM64) || defined(_M_ARM)

            // MSVC ARM/ARM64 has no portable MSVC intrinsic equivalent
            // that we can rely on here.
            inline void prefetch(const void*) noexcept
            {
            }

        #else

            inline void prefetch(const void*) noexcept
            {
            }

        #endif

    #else

        // Unknown compiler: software prefetch unavailable.
        inline void prefetch(const void*) noexcept
        {
        }

    #endif
}