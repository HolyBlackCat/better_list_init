// To run on https://gcc.godbolt.org, paste following:
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_list_init/master/include/better_list_init.hpp>
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_list_init/master/include/tests.cpp>
// Or paste `better_list_init.hpp`, followed by this file.


#ifndef BETTERLISTINIT_CONFIG // This lets us run tests on godbolt easier, see below.
#include "better_list_init.hpp"
#endif


#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>



#define ASSERT(...) \
    do { \
        if (!bool(__VA_ARGS__)) \
        { \
            std::cout << "Check failed at " __FILE__ ":" << __LINE__ << ": " #__VA_ARGS__ "\n"; \
            better_list_init::detail::abort(); \
        } \
    } \
    while (false)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) \
        { \
            std::cout << "Check failed at " __FILE__ ":" << __LINE__ << ": " #a " == " #b ", expanded to " << (a) << " == " << (b) << "\n"; \
            better_list_init::detail::abort(); \
        } \
    } \
    while (false)

// Tests if `T` has `.begin()` and `.end()`.
template <typename T, typename = void>
struct HasBeginEnd : std::false_type {};
template <typename T>
struct HasBeginEnd<T, decltype(void(std::declval<T>().begin()), void(std::declval<T>().end()))> : std::true_type {};

// Get a `init<P...>` value from element types.
// Causes UB when called, intended only to instantiate templates.
template <typename ...P>
better_list_init::init<P...> &&invalid_init_list()
{
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
    static int dummy;
    return reinterpret_cast<better_list_init::init<P...> &&>(dummy);
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
}

// A bunch of list variations for us to check.
#define CHECKED_LIST_TYPES(X) \
    /* Target type         | Element types... */\
    /* Empty list. */\
    X(int,                  ) \
    /* Homogeneous lists. */\
    X(int,                        int  ) \
    X(int,                        int &) \
    X(int,                  const int  ) \
    X(int,                  const int &) \
    X(int,                        int  ,       int  ) \
    X(int,                        int &,       int &) \
    X(int,                  const int  , const int  ) \
    X(int,                  const int &, const int &) \
    /* Heterogeneous lists. */\
    X(int,                  int, const int) \
    X(int,                  int, int, int) \
    X(int,                  int, const int, int &, const int &) \
    X(std::unique_ptr<int>, std::nullptr_t &, std::unique_ptr<int>) \

// Try to explicitly instantiate the types from `CHECKED_LIST_TYPES`.
#define CHECK_INSTANTIATION(target_, ...) \
    template class better_list_init::init<__VA_ARGS__>; \
    template class better_list_init::init<__VA_ARGS__>::elem_iter<target_>; \
    template class better_list_init::init<__VA_ARGS__>::elem_ref<target_>;
CHECKED_LIST_TYPES(CHECK_INSTANTIATION)
#undef CHECK_INSTANTIATION

// Check iterator categories for `CHECKED_LIST_TYPES`.
template <typename T>
struct IteratorCategoryChecker
{
    using value_type = T;

    template <typename U>
    IteratorCategoryChecker(U, U)
    {
        #ifdef __cpp_lib_ranges
        static_assert(std::random_access_iterator<U>, "The iterator concept wasn't satisfied.");
        #endif
        static_assert(std::is_same<typename std::iterator_traits<U>::iterator_category, std::random_access_iterator_tag>::value, "Wrong iterator category.");
    }
};
#define CHECK_ITERATOR_CATEGORY(target_, ...) (void)IteratorCategoryChecker<target_>(invalid_init_list<__VA_ARGS__>());
void check_iterator_categories()
{
    if (false) // Just instantiation is enough.
    {
        CHECKED_LIST_TYPES(CHECK_ITERATOR_CATEGORY)
    }
}
#undef CHECK_ITERATOR_CATEGORY

// A fake container, checks iterator sanity.
struct IteratorSanityChecker
{
    using value_type = int;

    template <typename T>
    IteratorSanityChecker(T begin, T end)
    {
        { // Increments and decrements.
            auto iter = begin;
            ASSERT_EQ(int(*iter++), 1);
            iter = begin;
            ASSERT_EQ(int(*++iter), 2);

            iter = begin + 1;
            ASSERT_EQ(int(*iter--), 2);
            iter = begin + 1;
            ASSERT_EQ(int(*--iter), 1);
        }

        { // +, -
            ASSERT_EQ(end - begin, 3);
            ASSERT_EQ(begin - end, -3);
            ASSERT_EQ(begin + 2 - end, -1);
            ASSERT_EQ(2 + begin - end, -1);
            ASSERT_EQ(end - 2 - begin, 1);

            auto iter = begin;
            ASSERT((iter += 2) - end == -1 && iter - end == -1);
            iter = end;
            ASSERT((iter -= 2) - begin == 1 && iter - begin == 1);
        }

        { // Indexing.
            ASSERT_EQ(int(*(begin)), 1);
            ASSERT_EQ(int(*(begin + 0)), 1);
            ASSERT_EQ(int(*(begin + 1)), 2);
            ASSERT_EQ(int(*(begin + 2)), 3);
            ASSERT_EQ(int(*(end - 3)), 1);
            ASSERT_EQ(int(*(end - 2)), 2);
            ASSERT_EQ(int(*(end - 1)), 3);

            ASSERT_EQ(int(begin[0]), 1);
            ASSERT_EQ(int(begin[1]), 2);
            ASSERT_EQ(int(begin[2]), 3);
            ASSERT_EQ(int(end[-3]), 1);
            ASSERT_EQ(int(end[-2]), 2);
            ASSERT_EQ(int(end[-1]), 3);
        }

        { // Comparison operators.
            auto a = begin;
            auto b = begin + 1;
            auto c = begin + 2;

            ASSERT(a != b && b == b && c != b);
            ASSERT(a < b && !(b < a) && !(b < b) && b < c && !(c < b));
            ASSERT(b > a && !(a > b) && !(b > b) && c > b && !(b > c));
            ASSERT(a <= b && !(b <= a) && b <= b && b <= c && !(c <= b));
            ASSERT(b >= a && !(a >= b) && b >= b && c >= b && !(b >= c));
        }
    }
};

template <typename ...P>
struct BasicExplicitRange
{
    using value_type = int;

    template <typename T>
    explicit BasicExplicitRange(T, T, P...) {}
};
template <typename ...P>
struct BasicImplicitRange
{
    using value_type = int;

    template <typename T>
    BasicImplicitRange(T, T, P...) {}
};
using ExplicitRange = BasicExplicitRange<>;
using ImplicitRange = BasicImplicitRange<>;
using ExplicitRangeWithArgs = BasicExplicitRange<int &&, int &&, int &&>;
using ImplicitRangeWithArgs = BasicImplicitRange<int &&, int &&, int &&>;

struct ExplicitNonRange
{
    explicit ExplicitNonRange(int, int) {}
};
struct ImplicitNonRange
{
    ImplicitNonRange(int, int) {}
};


template <typename T, typename ...P>
struct ConstexprRange
{
    using value_type = T;
    value_type sum = 0;

    // I tried to find the sum using the dummy array trick, but MSVC gets an ICE on it. D:<
    constexpr int calc_sum() {return 0;}
    template <typename ...Q>
    constexpr int calc_sum(int first, Q ...next) {return first + (calc_sum)(next...);}

    template <typename U>
    constexpr ConstexprRange(U begin, U end, P ...extra)
    {
        while (begin != end)
            sum += *begin++;

        sum += calc_sum(extra...);
    }
};

int main()
{
    // Iterator sanity tests.
    (void)IteratorSanityChecker(init{1, 2, 3});

    { // Generic usage tests.
        std::vector<std::unique_ptr<int>> vec1 = init{std::unique_ptr<int>(), std::make_unique<int>(42)};
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::unique_ptr<int>> vec2 = init{nullptr, std::make_unique<int>(42)};
        ASSERT(vec2[0] == nullptr);
        ASSERT(vec2[1] != nullptr && *vec2[1] == 42);

        std::vector<std::unique_ptr<int>> vec3 = init{nullptr};
        ASSERT_EQ(vec3.size(), 1);
        ASSERT(vec3[0] == nullptr);

        std::vector<std::unique_ptr<int>> vec4 = init{};
        ASSERT(vec4.empty());

        // Even without mandatory copy elision, homogeneous lists of non-movable types are fine.
        std::vector<std::atomic_int> vec5 = init{1, 2, 3};
        ASSERT_EQ(vec5.size(), 3);
        ASSERT_EQ(vec5[0].load(), 1);
        ASSERT_EQ(vec5[1].load(), 2);
        ASSERT_EQ(vec5[2].load(), 3);

        // Empty lists count as hetergeneous, thus require mandatory copy elision.
        std::vector<std::atomic_int> vec6 = init{};
        ASSERT(vec6.empty());

        int a = 5;
        const int b = 6;
        std::vector<std::atomic_int> vec7 = init{4, a, b};
        ASSERT_EQ(vec7.size(), 3);
        ASSERT_EQ(vec7[0].load(), 4);
        ASSERT_EQ(vec7[1].load(), 5);
        ASSERT_EQ(vec7[2].load(), 6);
    }

    { // Explicit initialization.
        // Double parentheses, because GCC 11 becomes confused otherwise. Most probably a bug?
        std::vector<std::unique_ptr<int>> vec1((init{nullptr, std::make_unique<int>(42)}));
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);
    }

    { // Initialization with extra arguments.
        std::vector<std::unique_ptr<int>> vec1 = init{nullptr, std::make_unique<int>(42)}.and_with(std::allocator<std::unique_ptr<int>>{});
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::atomic_int> vec2 = init{1, 2, 3}.and_with(std::allocator<std::unique_ptr<int>>{});
        ASSERT_EQ(vec2.size(), 3);
        ASSERT_EQ(vec2[0].load(), 1);
        ASSERT_EQ(vec2[1].load(), 2);
        ASSERT_EQ(vec2[2].load(), 3);
    }

    { // Non-range initialization.
        std::array<std::unique_ptr<int>, 2> arr1 = init{std::unique_ptr<int>(), std::make_unique<int>(42)};
        ASSERT(arr1[0] == nullptr);
        ASSERT(arr1[1] != nullptr && *arr1[1] == 42);

        std::array<std::unique_ptr<int>, 2> arr2 = init{nullptr, std::make_unique<int>(42)};
        ASSERT(arr2[0] == nullptr);
        ASSERT(arr2[1] != nullptr && *arr2[1] == 42);

        std::array<std::unique_ptr<int>, 2> arr3 = init{std::make_unique<int>(43)};
        ASSERT(arr3[0] != nullptr && *arr3[0] == 43);
        ASSERT(arr3[1] == nullptr);

        std::array<std::unique_ptr<int>, 0> arr4 = init{};
        (void)arr4;

        // The lists being homogeneous doesn't help us here, because we need to elide the move for the whole `std::array`.
        std::array<std::atomic_int, 3> arr5 = init{1, 2, 3};
        ASSERT_EQ(arr5[0].load(), 1);
        ASSERT_EQ(arr5[1].load(), 2);
        ASSERT_EQ(arr5[2].load(), 3);

        std::array<std::atomic_int, 0> arr6 = init{};
        (void)arr6;
    }

    { // Maps.
        { // From lists of pairs.
            // Homogeneous lists.
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map1 = init{
                std::make_pair(std::make_unique<int>(1), std::make_unique<float>(2.3f)),
                std::make_pair(std::make_unique<int>(2), std::make_unique<float>(3.4f)),
            };
            ASSERT_EQ(map1.size(), 2);
            for (const auto &elem : map1)
            {
                if (*elem.first == 1)
                    ASSERT_EQ(*elem.second, 2.3f);
                else
                    ASSERT_EQ(*elem.second, 3.4f);
            }

            std::map<std::atomic_int, std::atomic_int> map2 = init{
                std::make_pair(1, 2),
                std::make_pair(3, 4),
            };
            ASSERT_EQ(map2.size(), 2);
            ASSERT_EQ(map2.at(1).load(), 2);
            ASSERT_EQ(map2.at(3).load(), 4);

            // Heterogeneous lists.
            // For maps, the key is never movable (because the first template argument of the pair is const),
            // so we need the mandatory copy elision if the key is not copyable (or if the value is not movable, of course).
            std::map<int, std::unique_ptr<float>> map3 = init{
                std::make_pair(short(1), std::make_unique<float>(2.3f)),
                std::make_pair(2, std::make_unique<float>(3.4f)),
            };
            ASSERT_EQ(map3.size(), 2);
            ASSERT_EQ(*map3.at(1), 2.3f);
            ASSERT_EQ(*map3.at(2), 3.4f);

            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map4 = init{
                std::make_pair(nullptr, std::make_unique<float>(2.3f)),
                std::make_pair(std::make_unique<int>(2), std::make_unique<float>(3.4f)),
            };
            ASSERT_EQ(map4.size(), 2);
            for (const auto &elem : map4)
            {
                if (!elem.first)
                {
                    ASSERT_EQ(*elem.second, 2.3f);
                }
                else
                {
                    ASSERT_EQ(*elem.first, 2);
                    ASSERT_EQ(*elem.second, 3.4f);
                }
            }

            std::map<std::atomic_int, std::atomic_int> map5 = init{
                std::make_pair(short(1), 2),
                std::make_pair(3, 4)
            };
            ASSERT_EQ(map5.size(), 2);
            ASSERT_EQ(map5.at(1).load(), 2);
            ASSERT_EQ(map5.at(3).load(), 4);
        }

        { // From lists of lists.
            // If the map key is not copyable, all of this requires mandatory copy elision, even if the list is homogeneous,
            // since constructing a non-movable non-range (`std::pair<const A, B>` in this case) from a list
            // from a list requires mandatory copy elision, because we need to return it from a function.

            // Homogeneous lists.
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map1 = init{
                init{std::make_unique<int>(1), std::make_unique<float>(2.3f)},
                init{std::make_unique<int>(2), std::make_unique<float>(3.4f)},
            };
            ASSERT_EQ(map1.size(), 2);
            for (const auto &elem : map1)
            {
                if (*elem.first == 1)
                    ASSERT_EQ(*elem.second, 2.3f);
                else
                    ASSERT_EQ(*elem.second, 3.4f);
            }

            std::map<std::atomic_int, std::atomic_int> map2 = init{
                init{1, 2},
                init{3, 4},
            };
            ASSERT_EQ(map2.size(), 2);
            ASSERT_EQ(map2.at(1).load(), 2);
            ASSERT_EQ(map2.at(3).load(), 4);

            // Heterogeneous lists.

            // Here the map key type is copyable, so we don't need the mandatory copy elision.
            std::map<int, std::unique_ptr<float>> map3 = init{
                init{short(1), std::make_unique<float>(2.3f)},
                init{2, std::make_unique<float>(3.4f)},
            };
            ASSERT_EQ(map3.size(), 2);
            ASSERT_EQ(*map3.at(1), 2.3f);
            ASSERT_EQ(*map3.at(2), 3.4f);

            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map4 = init{
                init{nullptr, std::make_unique<float>(2.3f)},
                init{std::make_unique<int>(2), std::make_unique<float>(3.4f)},
            };
            ASSERT_EQ(map4.size(), 2);
            for (const auto &elem : map4)
            {
                if (!elem.first)
                {
                    ASSERT_EQ(*elem.second, 2.3f);
                }
                else
                {
                    ASSERT_EQ(*elem.first, 2);
                    ASSERT_EQ(*elem.second, 3.4f);
                }
            }

            std::map<std::atomic_int, std::atomic_int> map6 = init{
                init{short(1), 2},
                init{3, 4},
            };
            ASSERT_EQ(map6.size(), 2);
            ASSERT_EQ(map6.at(1).load(), 2);
            ASSERT_EQ(map6.at(3).load(), 4);
        }
    }

    { // Sets.
        // The sets are special, because they have const elements.
        std::set<int> set1 = init{1, 2, 3};
        ASSERT_EQ(set1.size(), 3);
        ASSERT(set1.count(1));
        ASSERT(set1.count(2));
        ASSERT(set1.count(3));

        std::set<std::atomic_int> set2 = init{1, 2, 3};
        ASSERT_EQ(set2.size(), 3);
        ASSERT(set2.count(1));
        ASSERT(set2.count(2));
        ASSERT(set2.count(3));
    }

    { // Explicit and implicit construction.
        // Range, explicit constructor.
        static_assert(!std::is_convertible<decltype(init{1, 2}), ExplicitRange>::value, "");
        static_assert(std::is_constructible<ExplicitRange, decltype(init{1, 2})>::value, "");
        static_assert(!std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ExplicitRange>::value, "");
        static_assert(!std::is_constructible<ExplicitRange, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");

        // Range, implicit constructor.
        static_assert(std::is_convertible<decltype(init{1, 2}), ImplicitRange>::value, "");
        static_assert(std::is_constructible<ImplicitRange, decltype(init{1, 2})>::value, "");
        static_assert(!std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ImplicitRange>::value, "");
        static_assert(!std::is_constructible<ImplicitRange, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");

        // Range with extra args, explicit constructor.
        static_assert(!std::is_convertible<decltype(init{1, 2}), ExplicitRangeWithArgs>::value, "");
        static_assert(!std::is_constructible<ExplicitRangeWithArgs, decltype(init{1, 2})>::value, "");
        static_assert(!std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ExplicitRangeWithArgs>::value, "");
        static_assert(std::is_constructible<ExplicitRangeWithArgs, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");

        // Range with extra args, implicit constructor.
        static_assert(!std::is_convertible<decltype(init{1, 2}), ImplicitRangeWithArgs>::value, "");
        static_assert(!std::is_constructible<ImplicitRangeWithArgs, decltype(init{1, 2})>::value, "");
        static_assert(std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ImplicitRangeWithArgs>::value, "");
        static_assert(std::is_constructible<ImplicitRangeWithArgs, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");

        // Non-range, explicit constructor.
        static_assert(!std::is_convertible<decltype(init{1, 2}), ExplicitNonRange>::value, "");
        static_assert(std::is_constructible<ExplicitNonRange, decltype(init{1, 2})>::value, "");
        static_assert(!std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ExplicitNonRange>::value, "");
        static_assert(!std::is_constructible<ExplicitNonRange, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");

        // Non-range, implicit constructor.
        static_assert(std::is_convertible<decltype(init{1, 2}), ImplicitNonRange>::value, "");
        static_assert(std::is_constructible<ImplicitNonRange, decltype(init{1, 2})>::value, "");
        static_assert(!std::is_convertible<decltype(init{1, 2}.and_with(1, 2, 3)), ImplicitNonRange>::value, "");
        static_assert(!std::is_constructible<ImplicitNonRange, decltype(init{1, 2}.and_with(1, 2, 3))>::value, "");
    }

    { // Make sure `.and_with(...)` doesn't work for non-ranges.
        // This is an arbitrary restriction, intended to force a cleaner usage.
        static_assert(std::is_constructible<std::array<int, 3>, decltype(init{1, 2, 3})>::value, "");
        static_assert(std::is_constructible<std::array<int, 3>, decltype(init{1, 2, 3}.and_with())>::value, ""); // Empty argument list is allowed, to simplify generic code.
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(init{1, 2}.and_with(3))>::value, "");
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(init{1}.and_with(2, 3))>::value, "");
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(init{}.and_with(1, 2, 3))>::value, "");
    }

    { // Constexpr-ness.
        constexpr int x1 = 1;
        constexpr float x2 = 2.1f;
        constexpr double x3 = 3.2;

        // Homogeneous.
        static_assert(ConstexprRange<int>(init{1, 2, 3}).sum == 6, "");
        static_assert(ConstexprRange<int>(init{x1, x1, x1}).sum == 3, "");
        // With extra args.
        static_assert(ConstexprRange<int, const double &, int>(init{1, 2, 3}.and_with(x3, 4)).sum == 13, "");
        static_assert(ConstexprRange<int, const double &, int>(init{x1, x1, x1}.and_with(x3, 4)).sum == 10, "");

        // Heterogeneous.
        static_assert(ConstexprRange<int>(init{1, 2.1f, 3.2}).sum == 6, "");
        static_assert(ConstexprRange<int>(init{x1, x2, x3}).sum == 6, "");
        // With extra args.
        static_assert(ConstexprRange<int, const double &, int>(init{1, 2.1f, 3.2}.and_with(x3, 4)).sum == 13, "");
        static_assert(ConstexprRange<int, const double &, int>(init{x1, x2, x3}.and_with(x3, 4)).sum == 13, "");
    }

    { // Nested lists with explicit constructors.
        // Non-range element.
        std::vector<ExplicitNonRange> vec1 = init{init{1,2}, init{3,4}, init{5,6}};
        ASSERT_EQ(vec1.size(), 3);
        // Non-range element, heterogeneous list.
        std::vector<ExplicitNonRange> vec2 = init{init{1,2}, init{3,4}, init{ExplicitNonRange(1,2)}};
        ASSERT_EQ(vec2.size(), 3);

        // Range element.
        std::vector<ExplicitRange> vec3 = init{init{1,2}, init{3,4}, init{5,6}};
        ASSERT_EQ(vec3.size(), 3);
        // Range element, heterogeneous list.
        std::vector<ExplicitRange> vec4 = init{init{1,2}, init{3,4}, init{5,6,7}};
        ASSERT_EQ(vec4.size(), 3);
    }

    { // Copyability of lists, and ref-qualifiers of conversion operators.
        int x = 42;
        float y = 43;

        using HomogeneousLvalue = decltype(init{x, x, x});
        using HomogeneousLvalueExtra = decltype(init{x, x, x}.and_with(42, 43, 44));
        // Copyability.
        static_assert(std::is_copy_constructible<HomogeneousLvalue>::value && std::is_copy_assignable<HomogeneousLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_convertible<HomogeneousLvalue &, ExplicitRange>::value && std::is_constructible<ExplicitRange, HomogeneousLvalue &>::value, "");
        static_assert(std::is_convertible<HomogeneousLvalue &, ImplicitRange>::value && std::is_constructible<ImplicitRange, HomogeneousLvalue &>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_convertible<HomogeneousLvalueExtra &, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HomogeneousLvalueExtra &>::value, "");
        static_assert(std::is_convertible<HomogeneousLvalueExtra &, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HomogeneousLvalueExtra &>::value, "");

        using HeterogeneousLvalue = decltype(init{x, x, y});
        using HeterogeneousLvalueExtra = decltype(init{x, x, y}.and_with(42, 43, 44));
        // Copyability.
        static_assert(std::is_copy_constructible<HeterogeneousLvalue>::value && std::is_copy_assignable<HeterogeneousLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_convertible<HeterogeneousLvalue &, ExplicitRange>::value && std::is_constructible<ExplicitRange, HeterogeneousLvalue &>::value, "");
        static_assert(std::is_convertible<HeterogeneousLvalue &, ImplicitRange>::value && std::is_constructible<ImplicitRange, HeterogeneousLvalue &>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_convertible<HeterogeneousLvalueExtra &, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HeterogeneousLvalueExtra &>::value, "");
        static_assert(std::is_convertible<HeterogeneousLvalueExtra &, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HeterogeneousLvalueExtra &>::value, "");

        using HomogeneousNonLvalue = decltype(init{42, x, x});
        using HomogeneousNonLvalueExtra = decltype(init{42, x, x}.and_with(42, 43, 44));
        // Copyability.
        static_assert(!std::is_copy_constructible<HomogeneousNonLvalue>::value && !std::is_copy_assignable<HomogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRange, HomogeneousNonLvalue &>::value && !std::is_convertible<HomogeneousNonLvalue, ExplicitRange>::value && std::is_constructible<ExplicitRange, HomogeneousNonLvalue>::value, "");
        static_assert(!std::is_constructible<ImplicitRange, HomogeneousNonLvalue &>::value && std::is_convertible<HomogeneousNonLvalue, ImplicitRange>::value && std::is_constructible<ImplicitRange, HomogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRangeWithArgs, HomogeneousNonLvalueExtra &>::value && !std::is_convertible<HomogeneousNonLvalueExtra, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HomogeneousNonLvalueExtra>::value, "");
        static_assert(!std::is_constructible<ImplicitRangeWithArgs, HomogeneousNonLvalueExtra &>::value && std::is_convertible<HomogeneousNonLvalueExtra, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HomogeneousNonLvalueExtra>::value, "");

        using HeterogeneousNonLvalue = decltype(init{42, x, y});
        using HeterogeneousNonLvalueExtra = decltype(init{42, x, y}.and_with(42, 43, 44));
        // Copyability.
        static_assert(!std::is_copy_constructible<HeterogeneousNonLvalue>::value && !std::is_copy_assignable<HeterogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRange, HeterogeneousNonLvalue &>::value && !std::is_convertible<HeterogeneousNonLvalue, ExplicitRange>::value && std::is_constructible<ExplicitRange, HeterogeneousNonLvalue>::value, "");
        static_assert(!std::is_constructible<ImplicitRange, HeterogeneousNonLvalue &>::value && std::is_convertible<HeterogeneousNonLvalue, ImplicitRange>::value && std::is_constructible<ImplicitRange, HeterogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRangeWithArgs, HeterogeneousNonLvalueExtra &>::value && !std::is_convertible<HeterogeneousNonLvalueExtra, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HeterogeneousNonLvalueExtra>::value, "");
        static_assert(!std::is_constructible<ImplicitRangeWithArgs, HeterogeneousNonLvalueExtra &>::value && std::is_convertible<HeterogeneousNonLvalueExtra, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HeterogeneousNonLvalueExtra>::value, "");
    }

    { // Begin/end iterators in homogeneous lists.
        int x = 3, y = 2, z = 1;
        // Lvalue-only lists have unconstrained begin/end.
        static_assert(HasBeginEnd<decltype(init{x, y, z})>::value, "");
        static_assert(HasBeginEnd<decltype(init{x, y, z}) &>::value, "");
        // Non-lvalue-only lists have rvalue-ref-qualified begin/end.
        static_assert(HasBeginEnd<decltype(init{1, 2, 3})>::value, "");
        static_assert(!HasBeginEnd<decltype(init{1, 2, 3}) &>::value, "");
        // Heterogeneous lists don't have begin/end.
        static_assert(!HasBeginEnd<decltype(init{x, 2, 3})>::value, "");
        static_assert(!HasBeginEnd<decltype(init{x, 2, 3}) &>::value, "");

        // Check that `std::sort` likes our iterators.
        auto i = init{x, y, z};
        std::sort(i.begin(), i.end());
        ASSERT_EQ(x, 1);
        ASSERT_EQ(y, 2);
        ASSERT_EQ(z, 3);
    }

    { // `list(list)` deduction, implicit.
        std::vector<std::vector<int>> v = init{init{}};
        ASSERT_EQ(v.size(), 1);
        ASSERT_EQ(v[0].size(), 0);
    }

    { // `list(list)` deduction, explicit.
        std::vector<std::vector<int>> v(init{init{}});
        ASSERT_EQ(v.size(), 1);
        ASSERT_EQ(v[0].size(), 0);
    }

    std::cout << "OK\n";
}
