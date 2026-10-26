#include <cstdint>
#include <concepts>
#include <functional>
#include <type_traits>
#include <vector>
#include <array>
#include <limits>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <numeric>
#include "Concepts/Integer.hpp"
#include "Concepts/StdVector.hpp"

namespace Carbo
{
    template<StdVector T>
    requires UnsignedInteger<typename T::value_type>
    void radixSort(T& array)
    {
        using U = typename T::value_type;

        constexpr size_t Radix = 256;
        constexpr U RadixMask = 0xFF;
        constexpr size_t NumBits = std::numeric_limits<U>::digits;

        T output(array.size());
        std::array<size_t, Radix> count{};

        for (unsigned shift = 0; shift < NumBits; shift += 8)
        {
            count.fill(0);

            // Count byte frequencies.
            for (auto value : array)
                ++count[(value >> shift) & RadixMask];

            // Compute cumulative positions.
            for (size_t i = 1; i < Radix; ++i)
                count[i] += count[i - 1];

            // Stable counting sort.
            for (size_t i = array.size(); i-- > 0;)
            {
                U value = array[i];
                size_t digit = static_cast<size_t>((value >> shift) & RadixMask);
                output[--count[digit]] = value;
            }

            array.swap(output);
        }
    }

    template<StdVector T>
    requires SignedInteger<typename T::value_type>
    void radixSort(T& array)
    {
        using U = typename T::value_type;
        using UnsignedU = std::make_unsigned_t<U>;

        constexpr size_t Radix = 256;
        constexpr UnsignedU RadixMask = 0xFF;
        constexpr size_t NumBits = std::numeric_limits<UnsignedU>::digits;
        constexpr UnsignedU SignMask = UnsignedU{1} << (NumBits - 1);

        T output(array.size());
        std::array<size_t, Radix> count{};

        for (unsigned shift = 0; shift < NumBits; shift += 8)
        {
            count.fill(0);

            // Count byte frequencies.
            for (U value : array)
            {
                const UnsignedU key = static_cast<UnsignedU>(value) ^ SignMask;

                ++count[(key >> shift) & RadixMask];
            }

            // Compute cumulative positions.
            for (size_t i = 1; i < Radix; ++i)
                count[i] += count[i - 1];

            // Stable counting sort.
            for (size_t i = array.size(); i-- > 0;)
            {
                const U value = array[i];
                const UnsignedU key = static_cast<UnsignedU>(value) ^ SignMask;

                const size_t digit = static_cast<size_t>((key >> shift) & RadixMask);
                output[--count[digit]] = value;
            }

            array.swap(output);
        }
    }

    template<StdVector T, typename GetInteger>
    requires std::invocable<GetInteger&, const typename T::value_type&> && std::integral<std::invoke_result_t<GetInteger&, const typename T::value_type&>>
    void radixSort(T& array, GetInteger getInteger)
    {
        using Value = typename T::value_type;
        using Integer =
            std::invoke_result_t<GetInteger&, const Value&>;
        using U = std::make_unsigned_t<Integer>;

        constexpr std::size_t Radix = 256;
        constexpr U RadixMask = 0xFF;
        constexpr unsigned NumBits = std::numeric_limits<U>::digits;

        if (array.size() < 2)
            return;

        constexpr bool IsSigned = std::signed_integral<Integer>;
        constexpr U SignMask = IsSigned ? (U{1} << (NumBits - 1)) : U{0};

        // Store each element's original index and normalized integer key.
        std::vector<std::size_t> indices(array.size());
        std::vector<std::size_t> indicesOut(array.size());
        std::vector<U> keys(array.size());

        std::iota(indices.begin(), indices.end(), std::size_t{0});

        for (std::size_t i = 0; i < array.size(); ++i)
        {
            U key = static_cast<U>(std::invoke(getInteger, array[i]));

            if constexpr (IsSigned)
                key ^= SignMask;

            keys[i] = key;
        }

        std::array<std::size_t, Radix> count{};

        for (unsigned shift = 0; shift < NumBits; shift += 8)
        {
            count.fill(0);

            // Count byte frequencies.
            for (std::size_t index : indices)
                ++count[(keys[index] >> shift) & RadixMask];

            // Compute cumulative positions.
            for (std::size_t i = 1; i < Radix; ++i)
                count[i] += count[i - 1];

            // Stable counting sort of indices.
            for (std::size_t i = indices.size(); i-- > 0;)
            {
                const std::size_t index = indices[i];
                const std::size_t digit =
                    static_cast<std::size_t>((keys[index] >> shift) & RadixMask);

                indicesOut[--count[digit]] = index;
            }

            indices.swap(indicesOut);
        }

        // Reorder the original elements according to the sorted indices.
        T output(array.size());

        for (std::size_t i = 0; i < indices.size(); ++i)
            output[i] = std::move(array[indices[i]]);

        array.swap(output);
    }
}