#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "Functions/RadixSort/RadixSort.hpp"

namespace
{
    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    template<typename T>
    std::vector<T> randomVector(std::mt19937_64& rng, size_t n)
    {
        std::uniform_int_distribution<T> dist(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
        std::vector<T> v(n);
        for (auto& x : v) x = dist(rng);
        return v;
    }

    template<typename T>
    void expectSortsLikeStd(std::vector<T> v)
    {
        auto expected = v;
        std::sort(expected.begin(), expected.end());
        Carbo::radixSort(v);
        EXPECT_EQ(v, expected);
    }

    template<typename T>
    class RadixSortIntegerTest : public ::testing::Test {};

    using IntegerTypes = ::testing::Types<uint8_t, uint16_t, uint32_t, uint64_t, int8_t, int16_t, int32_t, int64_t>;
    TYPED_TEST_SUITE(RadixSortIntegerTest, IntegerTypes);
}

// -----------------------------------------------------------------------------
// Integer overloads
// -----------------------------------------------------------------------------

TYPED_TEST(RadixSortIntegerTest, EmptyAndTiny)
{
    using T = TypeParam;
    expectSortsLikeStd<T>({});
    expectSortsLikeStd<T>({T(5)});
    expectSortsLikeStd<T>({T(2), T(1)});
}

TYPED_TEST(RadixSortIntegerTest, Extremes)
{
    using T = TypeParam;
    using L = std::numeric_limits<T>;
    expectSortsLikeStd<T>({L::max(), L::min(), T(0), T(1), L::max(), L::min()});
}

TYPED_TEST(RadixSortIntegerTest, AllEqual)
{
    expectSortsLikeStd<TypeParam>(std::vector<TypeParam>(100, TypeParam(7)));
}

TYPED_TEST(RadixSortIntegerTest, RandomSizes)
{
    std::mt19937_64 rng(12345);
    for (size_t n : {2, 3, 17, 255, 256, 257, 1000, 10000})
        expectSortsLikeStd<TypeParam>(randomVector<TypeParam>(rng, n));
}

TYPED_TEST(RadixSortIntegerTest, SortedAndReversed)
{
    std::mt19937_64 rng(1);
    auto v = randomVector<TypeParam>(rng, 1000);
    std::sort(v.begin(), v.end());
    expectSortsLikeStd<TypeParam>(v);
    std::reverse(v.begin(), v.end());
    expectSortsLikeStd<TypeParam>(v);
}

TYPED_TEST(RadixSortIntegerTest, ManyDuplicates)
{
    std::mt19937_64 rng(2);
    std::vector<TypeParam> v(5000);
    for (auto& x : v) x = static_cast<TypeParam>(rng() % 5);
    expectSortsLikeStd<TypeParam>(v);
}

TEST(RadixSortSignedTest, ByteBoundaries)
{
    expectSortsLikeStd<int32_t>({-1, 1, 0, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max(),
                                 -256, 256, -255, 255});
}

// -----------------------------------------------------------------------------
// Key extractor overload
// -----------------------------------------------------------------------------

namespace
{
    struct Item
    {
        int64_t key;
        int id;
    };

    struct NamedItem
    {
        uint16_t key;
        std::string name;
    };

    auto itemKey = [](const Item& i) { return i.key; };
}

TEST(RadixSortKeyExtractorTest, SignedKeysAreStable)
{
    std::mt19937_64 rng(99);
    std::vector<Item> items(5000);
    for (size_t i = 0; i < items.size(); ++i)
        items[i] = {static_cast<int64_t>(rng() % 200) - 100, static_cast<int>(i)};

    auto expected = items;
    std::stable_sort(expected.begin(), expected.end(), [](const Item& a, const Item& b) { return a.key < b.key; });

    Carbo::radixSort(items, itemKey);

    ASSERT_EQ(items.size(), expected.size());
    for (size_t i = 0; i < items.size(); ++i)
    {
        EXPECT_EQ(items[i].key, expected[i].key) << "at " << i;
        EXPECT_EQ(items[i].id, expected[i].id) << "stability at " << i;
    }
}

TEST(RadixSortKeyExtractorTest, UnsignedKeysWithStringPayload)
{
    std::mt19937_64 rng(7);
    std::vector<NamedItem> items;
    for (int i = 0; i < 2000; ++i)
        items.push_back({static_cast<uint16_t>(rng()), std::to_string(i)});

    auto expected = items;
    std::stable_sort(expected.begin(), expected.end(), [](const NamedItem& a, const NamedItem& b) { return a.key < b.key; });

    Carbo::radixSort(items, [](const NamedItem& i) { return i.key; });

    for (size_t i = 0; i < items.size(); ++i)
    {
        EXPECT_EQ(items[i].key, expected[i].key) << "at " << i;
        EXPECT_EQ(items[i].name, expected[i].name) << "at " << i;
    }
}

TEST(RadixSortKeyExtractorTest, EmptyAndSingle)
{
    std::vector<Item> empty;
    Carbo::radixSort(empty, itemKey);
    EXPECT_TRUE(empty.empty());

    std::vector<Item> one{{5, 0}};
    Carbo::radixSort(one, itemKey);
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0].id, 0);
}

TEST(RadixSortKeyExtractorTest, ExtremeKeys)
{
    std::vector<Item> v{{std::numeric_limits<int64_t>::max(), 0},
                        {std::numeric_limits<int64_t>::min(), 1},
                        {0, 2}, {-1, 3}, {1, 4}};
    Carbo::radixSort(v, itemKey);

    std::vector<int> ids;
    for (auto& i : v) ids.push_back(i.id);
    EXPECT_EQ(ids, (std::vector<int>{1, 3, 2, 4, 0}));
}

// -----------------------------------------------------------------------------
// Stress
// -----------------------------------------------------------------------------

namespace
{
    template<typename T>
    class RadixSortStressTest : public ::testing::Test {};

    TYPED_TEST_SUITE(RadixSortStressTest, IntegerTypes);

    template<typename F>
    double elapsedMs(F&& f)
    {
        const auto start = std::chrono::steady_clock::now();
        f();
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::milli>(end - start).count();
    }

    void printComparison(const char* name, size_t n, double radixMs, double stdMs)
    {
        std::printf("[ PERF ] %s n=%zu radixSort=%.3f ms std::sort=%.3f ms (ratio %.2fx)\n",
                    name, n, radixMs, stdMs, stdMs / radixMs);
    }
}

TYPED_TEST(RadixSortStressTest, LargeRandomArray)
{
    std::mt19937_64 rng(424242);
    const auto input = randomVector<TypeParam>(rng, 1u << 20);

    auto radixInput = input;
    auto stdInput = input;

    const double radixMs = elapsedMs([&] { Carbo::radixSort(radixInput); });
    const double stdMs = elapsedMs([&] { std::sort(stdInput.begin(), stdInput.end()); });

    EXPECT_EQ(radixInput, stdInput);
    printComparison(::testing::UnitTest::GetInstance()->current_test_info()->name(), input.size(), radixMs, stdMs);
}

TYPED_TEST(RadixSortStressTest, ManyRandomRounds)
{
    std::mt19937_64 rng(31337);
    std::uniform_int_distribution<size_t> sizeDist(0, 4096);

    for (int round = 0; round < 500; ++round)
    {
        const size_t n = sizeDist(rng);
        expectSortsLikeStd<TypeParam>(randomVector<TypeParam>(rng, n));
        if (testing::Test::HasFailure())
        {
            ADD_FAILURE() << "failed in round " << round << " with n = " << n;
            return;
        }
    }
}

TEST(RadixSortStressTest, LargeKeyExtractorIsStable)
{
    std::mt19937_64 rng(555);
    std::vector<Item> items(1u << 18);
    for (size_t i = 0; i < items.size(); ++i)
        items[i] = {static_cast<int64_t>(rng() % 1000) - 500, static_cast<int>(i)};

    auto expected = items;

    const double radixMs = elapsedMs([&] { Carbo::radixSort(items, itemKey); });
    const double stdMs = elapsedMs([&] {
        std::stable_sort(expected.begin(), expected.end(), [](const Item& a, const Item& b) { return a.key < b.key; });
    });

    ASSERT_EQ(items.size(), expected.size());
    size_t mismatches = 0;
    for (size_t i = 0; i < items.size(); ++i)
        mismatches += (items[i].key != expected[i].key) || (items[i].id != expected[i].id);
    EXPECT_EQ(mismatches, 0u);
    printComparison(::testing::UnitTest::GetInstance()->current_test_info()->name(), items.size(), radixMs, stdMs);
}
