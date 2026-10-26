#pragma once

#include <concepts>
#include <type_traits>

template<typename T>
concept Integer = std::is_integral_v<T>;

template<typename T>
concept UnsignedInteger = std::is_unsigned_v<T> && std::is_integral_v<T>;

template<typename T>
concept SignedInteger = std::is_signed_v<T> && std::is_integral_v<T>;