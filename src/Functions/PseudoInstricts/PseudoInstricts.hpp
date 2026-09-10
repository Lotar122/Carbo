#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
#endif

namespace Carbo
{
    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        static inline float _mm_horizontalmin_ps(__m128 v)
        {
            v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)));
            v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2)));
            return _mm_cvtss_f32(v);
        };

        static inline float _mm_horizontalmax_ps(__m128 v)
        {
            v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)));
            v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 0, 3, 2)));
            return _mm_cvtss_f32(v);
        };
    #else
        using __m128 = int;
        static inline float _mm_horizontalmin_ps(__m128 v) {};
        static inline float _mm_horizontalmax_ps(__m128 v) {};
    #endif
}