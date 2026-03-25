#pragma once

#include <iostream>
#include "Macros/DEBUG_BREAK.hpp"

#ifndef NDEBUG
    #define break_assert(expr)                                   \
        do {                                                     \
            if (!(expr)) {                                       \
                std::cerr << "Assertion failed: " #expr          \
                          << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
                DEBUG_BREAK();                                   \
            }                                                    \
        } while (0)
#else
    #define MY_ASSERT(expr) ((void)0)
#endif