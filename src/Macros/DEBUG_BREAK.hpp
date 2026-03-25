#pragma once

#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define DEBUG_BREAK() __builtin_trap()
#else
#include <cstdlib>
#define DEBUG_BREAK() std::abort()
#endif