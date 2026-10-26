#pragma once

#include <cstdint>

#if defined(_MSC_VER) || (defined(__clang__) && _WIN32)
    #include <intrin.h>
#endif

namespace Carbo
{
    class CPUFeatures
    {
    public:
        enum class feature
        {
            sse2,
            sse41,
            fma,
            avx,
            avx2,
            avx512
        };

    private:
        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
            inline static constexpr bool x86 = true;
        #else
            inline static constexpr bool x86 = false;
        #endif

        // ------------------------------------------------------------
        // x86 CPUID
        // ------------------------------------------------------------

        #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

            struct cpuid_result
            {
                int eax;
                int ebx;
                int ecx;
                int edx;
            };

            inline cpuid_result cpuid(int leaf, int subleaf = 0)
            {
                int regs[4];

                #if defined(_MSC_VER) || (defined(__clang__) && _WIN32)
                    __cpuidex(regs, leaf, subleaf);
                #else
                    __asm__ volatile(
                        "cpuid"
                        : "=a"(regs[0]),
                          "=b"(regs[1]),
                          "=c"(regs[2]),
                          "=d"(regs[3])
                        : "a"(leaf),
                          "c"(subleaf)
                    );
                #endif

                return {
                    regs[0],
                    regs[1],
                    regs[2],
                    regs[3]
                };
            }

            inline std::uint64_t xgetbv(unsigned int index)
            {
                #if defined(_MSC_VER) || (defined(__clang__) && _WIN32)
                    return _xgetbv(index);
                #else
                    std::uint32_t eax;
                    std::uint32_t edx;

                    __asm__ volatile(
                        "xgetbv"
                        : "=a"(eax),
                          "=d"(edx)
                        : "c"(index)
                    );

                    return
                        (static_cast<std::uint64_t>(edx) << 32) |
                        static_cast<std::uint64_t>(eax);
                #endif
            }

            // ------------------------------------------------------------
            // OS AVX state
            // ------------------------------------------------------------

            inline bool os_avx_support()
            {
                const auto leaf1 = cpuid(1);

                // OSXSAVE
                constexpr int osxsave_bit = 27;

                // AVX
                constexpr int avx_bit = 28;

                const bool osxsave =
                    (leaf1.ecx & (1 << osxsave_bit)) != 0;

                const bool avx =
                    (leaf1.ecx & (1 << avx_bit)) != 0;

                if (!osxsave || !avx)
                    return false;

                // XMM + YMM state must be enabled by the OS.
                //
                // XCR0:
                // bit 1 = XMM
                // bit 2 = YMM
                //
                constexpr std::uint64_t xmm_ymm =
                    (1ull << 1) | (1ull << 2);

                return (xgetbv(0) & xmm_ymm) == xmm_ymm;
            }

            // ------------------------------------------------------------
            // OS AVX-512 state
            // ------------------------------------------------------------

            inline bool os_avx512_support()
            {
                if (!os_avx_support())
                    return false;

                // AVX-512 requires:
                //
                // XMM       bit 1
                // YMM       bit 2
                // Opmask    bit 5
                // ZMM_hi256 bit 6
                // Hi16_ZMM  bit 7
                //
                constexpr std::uint64_t avx512_state =
                    (1ull << 1) |
                    (1ull << 2) |
                    (1ull << 5) |
                    (1ull << 6) |
                    (1ull << 7);

                return (xgetbv(0) & avx512_state) == avx512_state;
            }

            // ------------------------------------------------------------
            // CPU feature detection
            // ------------------------------------------------------------

            inline bool cpu_supports(feature f)
            {
                const auto leaf0 = cpuid(0);

                const unsigned max_leaf =
                    static_cast<unsigned>(leaf0.eax);

                if (max_leaf < 1)
                    return false;

                const auto leaf1 = cpuid(1);

                switch (f)
                {
                    case feature::sse2:
                        // CPUID.01H:EDX.SSE2[bit 26]
                        return (leaf1.edx & (1 << 26)) != 0;

                    case feature::sse41:
                        // CPUID.01H:ECX.SSE4.1[bit 19]
                        return (leaf1.ecx & (1 << 19)) != 0;

                    case feature::fma:
                        // CPUID.01H:ECX.FMA[bit 12]
                        //
                        // FMA uses the AVX register state, so the OS
                        // must support AVX state as well.
                        return (leaf1.ecx & (1 << 12)) != 0 &&
                            os_avx_support();

                    case feature::avx:
                        // CPUID + OSXSAVE + XCR0.
                        return os_avx_support();

                    case feature::avx2:
                    {
                        if (max_leaf < 7)
                            return false;

                        if (!os_avx_support())
                            return false;

                        const auto leaf7 = cpuid(7, 0);

                        // CPUID.07H:EBX.AVX2[bit 5]
                        return (leaf7.ebx & (1 << 5)) != 0;
                    }

                    case feature::avx512:
                    {
                        if (max_leaf < 7)
                            return false;

                        if (!os_avx512_support())
                            return false;

                        const auto leaf7 = cpuid(7, 0);

                        // CPUID.07H:EBX.AVX512F[bit 16]
                        return (leaf7.ebx & (1 << 16)) != 0;
                    }
                }

                return false;
            }

        #endif

    public:
        [[nodiscard]] inline bool supports(feature f)
        {
            if constexpr (!x86)
            {
                // These features are x86 SIMD extensions.
                return false;
            }
            else
            {
                #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)

                    return cpu_supports(f);

                #else

                    // Unknown architecture.
                    //
                    // Deliberately return false rather than guessing.
                    return false;

                #endif
            }
        }
    };
}