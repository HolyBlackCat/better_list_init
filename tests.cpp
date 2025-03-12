// To run on https://gcc.godbolt.org, paste following:
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_list_init/master/include/better_list_init.hpp>
// #include <https://raw.githubusercontent.com/HolyBlackCat/better_list_init/master/include/tests.cpp>
// Or paste `better_list_init.hpp`, followed by this file.


// Some code to emulate the MSVC's broken `std::construct_at()` on compilers other than MSVC.
// Our emulation appears to be weaker than the actual bug (doesn't trigger in all cases).
// Enable this and observe compile-time errors:
//     make STDLIB=libstdc++ STANDARD=20 CXXFLAGS='-DBREAK_CONSTRUCT_AT'
// Disable the respective tests and observe the lack of errors:
//     make STDLIB=libstdc++ STANDARD=20 CXXFLAGS='-DBREAK_CONSTRUCT_AT -DCONTAINERS_HAVE_MANDATORY_COPY_ELISION=0'
// Enable the allocator hack and observe the lack of errors:
//     make STDLIB=libstdc++ STANDARD=20 CXXFLAGS='-DBREAK_CONSTRUCT_AT -DBETTERLISTINIT_ALLOCATOR_HACK=2'
#if BREAK_CONSTRUCT_AT
// This has to be above all the includes, to correctly perform the override.

// Make sure we either don't use the allocator hack at all, or force-enable it,
//   since the compile-time test to conditionally enable it doesn't work
//   with our broken `std::construct_at()`, since we can't make it SFIANE-friendly.
#if BETTERLISTINIT_ALLOCATOR_HACK == 1
#error "Our broken `std::construct_at()` is not SFINAE-friendly, it doesn't work with `BETTERLISTINIT_ALLOCATOR_HACK == 1`. Set it to 2."
#endif
// Include something to identify the standard library,
// but without calling `std::construct_at` before we break it.
#include <version>
#ifdef __GLIBCXX__
namespace std _GLIBCXX_VISIBILITY(default)
{
    namespace better_list_init
    {
        template <typename T> T &&declval();
    }

    _GLIBCXX_BEGIN_NAMESPACE_VERSION
    template <typename T, typename U>
    constexpr auto construct_at(T *ptr, U &&param)
    noexcept(noexcept(::new((void *)nullptr) T(better_list_init::declval<U>())))
    -> decltype(::new((void *)nullptr) T(better_list_init::declval<U>()))
    {
        // .-- Check that the allocator hack didn't call this on a non-movable type.
        // v   We can't make this SFINAE-friendly, because then we'd just fall back to the stock `std::construct_at()`.
        static_assert(!(
            // If `T` is not move-constructible, and
            !requires{T(better_list_init::declval<T &&>());} &&
            // if `U::operator T()` is a thing.
            requires{param.operator T();}
        ), "Emulated `std::construct_at` bug!");
        return ::new((void*)ptr) T(static_cast<U &&>(param));
    }
    _GLIBCXX_END_NAMESPACE_VERSION
}
#else
// Note: there's no implementation for libc++ because the allocator hack doesn't work there anyway. See the comments on `BETTERLISTINIT_ALLOCATOR_HACK`.
#error "Don't know how to break `std::construct_at` for this standard library."
#endif
#endif


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


// Expands to the preferred init list notation for the current language standard.
#if BETTERLISTINIT_ALLOW_BRACES
#define INIT(...) BETTERLISTINIT_INIT{__VA_ARGS__}
#else
#define INIT(...) BETTERLISTINIT_INIT(__VA_ARGS__)
#endif


// Whether we have the mandatory copy elision.
#ifndef LANG_HAS_MANDATORY_COPY_ELISION
#if BETTERLISTINIT_CXX_STANDARD >= 17
#define LANG_HAS_MANDATORY_COPY_ELISION 1
#else
#define LANG_HAS_MANDATORY_COPY_ELISION 0
#endif
#endif

// Whether we have the mandatory copy elision (and no bugged `std::consturct_at()`) OR the allocator hack.
#ifndef CONTAINERS_HAVE_MANDATORY_COPY_ELISION
// Work around MSVC issue https://github.com/microsoft/STL/issues/2620, which breaks mandatory copy elision in C++20 mode.
#if BETTERLISTINIT_ALLOCATOR_HACK || (BETTERLISTINIT_CXX_STANDARD >= 17 && (!defined(_MSC_VER) || _MSC_VER > 1933 || BETTERLISTINIT_CXX_STANDARD < 20))
#define CONTAINERS_HAVE_MANDATORY_COPY_ELISION 1
#else
#define CONTAINERS_HAVE_MANDATORY_COPY_ELISION 0
#endif
#endif


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
better_list_init::type::BETTERLISTINIT_IDENTIFIER<P...> &&invalid_init_list()
{
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    #endif
    static int dummy;
    return reinterpret_cast<better_list_init::type::BETTERLISTINIT_IDENTIFIER<P...> &&>(dummy);
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
    template class better_list_init::type::BETTERLISTINIT_IDENTIFIER<__VA_ARGS__>; \
    template class better_list_init::type::BETTERLISTINIT_IDENTIFIER<__VA_ARGS__>::elem_iter<target_>; \
    template class better_list_init::type::BETTERLISTINIT_IDENTIFIER<__VA_ARGS__>::elem_ref<target_>;
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
        #if BETTERLISTINIT_CXX_STANDARD >= 20
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
struct BasicImplicitRange : public BasicExplicitRange<P...>
{
    using BasicExplicitRange<P...>::BasicExplicitRange;

    BasicImplicitRange(std::initializer_list<typename BasicImplicitRange::value_type>, P...);
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
    (void)IteratorSanityChecker(INIT(1, 2, 3));

    { // Generic usage tests.
        std::vector<std::unique_ptr<int>> vec1 = INIT(std::unique_ptr<int>(), std::make_unique<int>(42));
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::unique_ptr<int>> vec2 = INIT(nullptr, std::make_unique<int>(42));
        ASSERT(vec2[0] == nullptr);
        ASSERT(vec2[1] != nullptr && *vec2[1] == 42);

        std::vector<std::unique_ptr<int>> vec3 = INIT(nullptr);
        ASSERT_EQ(vec3.size(), 1);
        ASSERT(vec3[0] == nullptr);

        std::vector<std::unique_ptr<int>> vec4 = INIT();
        ASSERT(vec4.empty());

        // Even without mandatory copy elision, homogeneous lists of non-movable types are fine.
        std::vector<std::atomic_int> vec5 = INIT(1, 2, 3);
        ASSERT_EQ(vec5.size(), 3);
        ASSERT_EQ(vec5[0].load(), 1);
        ASSERT_EQ(vec5[1].load(), 2);
        ASSERT_EQ(vec5[2].load(), 3);

        #if CONTAINERS_HAVE_MANDATORY_COPY_ELISION
        // Empty lists count as hetergeneous, thus require mandatory copy elision.
        std::vector<std::atomic_int> vec6 = INIT();
        ASSERT(vec6.empty());

        int a = 5;
        const int b = 6;
        std::vector<std::atomic_int> vec7 = INIT(4, a, b);
        ASSERT_EQ(vec7.size(), 3);
        ASSERT_EQ(vec7[0].load(), 4);
        ASSERT_EQ(vec7[1].load(), 5);
        ASSERT_EQ(vec7[2].load(), 6);
        #endif
    }

    { // Explicit initialization.
        // Double parentheses, because GCC 11 becomes confused otherwise. Most probably a bug?
        std::vector<std::unique_ptr<int>> vec1((INIT(nullptr, std::make_unique<int>(42))));
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);
    }

    { // Initialization with extra arguments.
        std::vector<std::unique_ptr<int>> vec1 = INIT(nullptr, std::make_unique<int>(42)).and_with(std::allocator<std::unique_ptr<int>>{});
        ASSERT_EQ(vec1.size(), 2);
        ASSERT(vec1[0] == nullptr);
        ASSERT(vec1[1] != nullptr && *vec1[1] == 42);

        std::vector<std::atomic_int> vec2 = INIT(1, 2, 3).and_with(std::allocator<std::unique_ptr<int>>{});
        ASSERT_EQ(vec2.size(), 3);
        ASSERT_EQ(vec2[0].load(), 1);
        ASSERT_EQ(vec2[1].load(), 2);
        ASSERT_EQ(vec2[2].load(), 3);
    }

    { // Non-range initialization.
        std::array<std::unique_ptr<int>, 2> arr1 = INIT(std::unique_ptr<int>(), std::make_unique<int>(42));
        ASSERT(arr1[0] == nullptr);
        ASSERT(arr1[1] != nullptr && *arr1[1] == 42);

        std::array<std::unique_ptr<int>, 2> arr2 = INIT(nullptr, std::make_unique<int>(42));
        ASSERT(arr2[0] == nullptr);
        ASSERT(arr2[1] != nullptr && *arr2[1] == 42);

        std::array<std::unique_ptr<int>, 2> arr3 = INIT(std::make_unique<int>(43));
        ASSERT(arr3[0] != nullptr && *arr3[0] == 43);
        ASSERT(arr3[1] == nullptr);

        std::array<std::unique_ptr<int>, 0> arr4 = INIT();
        (void)arr4;

        // The lists being homogeneous doesn't help us here, because we need to elide the move for the whole `std::array`.
        // The bugged `std::construct_at()` doesn't affect this, and the allocator hack doesn't help here.
        #if LANG_HAS_MANDATORY_COPY_ELISION
        std::array<std::atomic_int, 3> arr5 = INIT(1, 2, 3);
        ASSERT_EQ(arr5[0].load(), 1);
        ASSERT_EQ(arr5[1].load(), 2);
        ASSERT_EQ(arr5[2].load(), 3);

        std::array<std::atomic_int, 0> arr6 = INIT();
        (void)arr6;
        #endif
    }

    { // Maps.
        { // From lists of pairs.
            // Homogeneous lists.
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map1 = INIT(
                std::make_pair(std::make_unique<int>(1), std::make_unique<float>(2.3f)),
                std::make_pair(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map1.size(), 2);
            for (const auto &elem : map1)
            {
                if (*elem.first == 1)
                    ASSERT_EQ(*elem.second, 2.3f);
                else
                    ASSERT_EQ(*elem.second, 3.4f);
            }

            std::map<std::atomic_int, std::atomic_int> map2 = INIT(
                std::make_pair(1, 2),
                std::make_pair(3, 4)
            );
            ASSERT_EQ(map2.size(), 2);
            ASSERT_EQ(map2.at(1).load(), 2);
            ASSERT_EQ(map2.at(3).load(), 4);

            // Heterogeneous lists.
            // For maps, the key is never movable (because the first template argument of the pair is const),
            // so we need the mandatory copy elision if the key is not copyable (or if the value is not movable, of course).
            std::map<int, std::unique_ptr<float>> map3 = INIT(
                std::make_pair(short(1), std::make_unique<float>(2.3f)),
                std::make_pair(2, std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map3.size(), 2);
            ASSERT_EQ(*map3.at(1), 2.3f);
            ASSERT_EQ(*map3.at(2), 3.4f);

            #if CONTAINERS_HAVE_MANDATORY_COPY_ELISION
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map4 = INIT(
                std::make_pair(nullptr, std::make_unique<float>(2.3f)),
                std::make_pair(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
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

            std::map<std::atomic_int, std::atomic_int> map5 = INIT(
                std::make_pair(short(1), 2),
                std::make_pair(3, 4)
            );
            ASSERT_EQ(map5.size(), 2);
            ASSERT_EQ(map5.at(1).load(), 2);
            ASSERT_EQ(map5.at(3).load(), 4);
            #endif
        }

        { // From lists of lists.
            // If the map key is not copyable, all of this requires mandatory copy elision, even if the list is homogeneous,
            // since constructing a non-movable non-range (`std::pair<const A, B>` in this case) from a list
            // from a list requires mandatory copy elision, because we need to return it from a function.

            // Homogeneous lists.
            #if CONTAINERS_HAVE_MANDATORY_COPY_ELISION
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map1 = INIT(
                INIT(std::make_unique<int>(1), std::make_unique<float>(2.3f)),
                INIT(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map1.size(), 2);
            for (const auto &elem : map1)
            {
                if (*elem.first == 1)
                    ASSERT_EQ(*elem.second, 2.3f);
                else
                    ASSERT_EQ(*elem.second, 3.4f);
            }

            std::map<std::atomic_int, std::atomic_int> map2 = INIT(
                INIT(1, 2),
                INIT(3, 4)
            );
            ASSERT_EQ(map2.size(), 2);
            ASSERT_EQ(map2.at(1).load(), 2);
            ASSERT_EQ(map2.at(3).load(), 4);
            #endif

            // Heterogeneous lists.

            // Here the map key type is copyable, so we don't need the mandatory copy elision.
            std::map<int, std::unique_ptr<float>> map3 = INIT(
                INIT(short(1), std::make_unique<float>(2.3f)),
                INIT(2, std::make_unique<float>(3.4f))
            );
            ASSERT_EQ(map3.size(), 2);
            ASSERT_EQ(*map3.at(1), 2.3f);
            ASSERT_EQ(*map3.at(2), 3.4f);

            #if CONTAINERS_HAVE_MANDATORY_COPY_ELISION
            std::map<std::unique_ptr<int>, std::unique_ptr<float>> map4 = INIT(
                INIT(nullptr, std::make_unique<float>(2.3f)),
                INIT(std::make_unique<int>(2), std::make_unique<float>(3.4f))
            );
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

            std::map<std::atomic_int, std::atomic_int> map6 = INIT(
                INIT(short(1), 2),
                INIT(3, 4)
            );
            ASSERT_EQ(map6.size(), 2);
            ASSERT_EQ(map6.at(1).load(), 2);
            ASSERT_EQ(map6.at(3).load(), 4);
            #endif
        }
    }

    { // Sets.
        // The sets are special, because they have const elements.
        std::set<int> set1 = INIT(1, 2, 3);
        ASSERT_EQ(set1.size(), 3);
        ASSERT(set1.count(1));
        ASSERT(set1.count(2));
        ASSERT(set1.count(3));

        std::set<std::atomic_int> set2 = INIT(1, 2, 3);
        ASSERT_EQ(set2.size(), 3);
        ASSERT(set2.count(1));
        ASSERT(set2.count(2));
        ASSERT(set2.count(3));
    }

    { // Explicit and implicit construction.
        // Range, explicit constructor.
        static_assert(!std::is_convertible<decltype(INIT(1, 2)), ExplicitRange>::value, "");
        static_assert(std::is_constructible<ExplicitRange, decltype(INIT(1, 2))>::value, "");
        static_assert(!std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ExplicitRange>::value, "");
        static_assert(!std::is_constructible<ExplicitRange, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");

        // Range, implicit constructor.
        static_assert(std::is_convertible<decltype(INIT(1, 2)), ImplicitRange>::value, "");
        static_assert(std::is_constructible<ImplicitRange, decltype(INIT(1, 2))>::value, "");
        static_assert(!std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ImplicitRange>::value, "");
        static_assert(!std::is_constructible<ImplicitRange, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");

        // Range with extra args, explicit constructor.
        static_assert(!std::is_convertible<decltype(INIT(1, 2)), ExplicitRangeWithArgs>::value, "");
        static_assert(!std::is_constructible<ExplicitRangeWithArgs, decltype(INIT(1, 2))>::value, "");
        static_assert(!std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ExplicitRangeWithArgs>::value, "");
        static_assert(std::is_constructible<ExplicitRangeWithArgs, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");

        // Range with extra args, implicit constructor.
        static_assert(!std::is_convertible<decltype(INIT(1, 2)), ImplicitRangeWithArgs>::value, "");
        static_assert(!std::is_constructible<ImplicitRangeWithArgs, decltype(INIT(1, 2))>::value, "");
        static_assert(std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ImplicitRangeWithArgs>::value, "");
        static_assert(std::is_constructible<ImplicitRangeWithArgs, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");

        // Non-range, explicit constructor.
        static_assert(!std::is_convertible<decltype(INIT(1, 2)), ExplicitNonRange>::value, "");
        static_assert(std::is_constructible<ExplicitNonRange, decltype(INIT(1, 2))>::value, "");
        static_assert(!std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ExplicitNonRange>::value, "");
        static_assert(!std::is_constructible<ExplicitNonRange, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");

        // Non-range, implicit constructor.
        static_assert(std::is_convertible<decltype(INIT(1, 2)), ImplicitNonRange>::value, "");
        static_assert(std::is_constructible<ImplicitNonRange, decltype(INIT(1, 2))>::value, "");
        static_assert(!std::is_convertible<decltype(INIT(1, 2).and_with(1, 2, 3)), ImplicitNonRange>::value, "");
        static_assert(!std::is_constructible<ImplicitNonRange, decltype(INIT(1, 2).and_with(1, 2, 3))>::value, "");
    }

    { // Make sure `.and_with(...)` doesn't work for non-ranges.
        // This is an arbitrary restriction, intended to force a cleaner usage.
        static_assert(std::is_constructible<std::array<int, 3>, decltype(INIT(1, 2, 3))>::value, "");
        static_assert(std::is_constructible<std::array<int, 3>, decltype(INIT(1, 2, 3).and_with())>::value, ""); // Empty argument list is allowed, to simplify generic code.
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(INIT(1, 2).and_with(3))>::value, "");
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(INIT(1).and_with(2, 3))>::value, "");
        static_assert(!std::is_constructible<std::array<int, 3>, decltype(INIT().and_with(1, 2, 3))>::value, "");
    }

    { // Constexpr-ness.
        constexpr int x1 = 1;
        constexpr float x2 = 2.1f;
        constexpr double x3 = 3.2;

        // Homogeneous.
        static_assert(ConstexprRange<int>(INIT(1, 2, 3)).sum == 6, "");
        static_assert(ConstexprRange<int>(INIT(x1, x1, x1)).sum == 3, "");
        // With extra args.
        static_assert(ConstexprRange<int, const double &, int>(INIT(1, 2, 3).and_with(x3, 4)).sum == 13, "");
        static_assert(ConstexprRange<int, const double &, int>(INIT(x1, x1, x1).and_with(x3, 4)).sum == 10, "");

        // Heterogeneous.
        static_assert(ConstexprRange<int>(INIT(1, 2.1f, 3.2)).sum == 6, "");
        static_assert(ConstexprRange<int>(INIT(x1, x2, x3)).sum == 6, "");
        // With extra args.
        static_assert(ConstexprRange<int, const double &, int>(INIT(1, 2.1f, 3.2).and_with(x3, 4)).sum == 13, "");
        static_assert(ConstexprRange<int, const double &, int>(INIT(x1, x2, x3).and_with(x3, 4)).sum == 13, "");
    }

    { // Nested lists with explicit constructors.
        // Non-range element.
        std::vector<ExplicitNonRange> vec1 = INIT(INIT(1,2), INIT(3,4), INIT(5,6));
        ASSERT_EQ(vec1.size(), 3);
        // Non-range element, heterogeneous list.
        std::vector<ExplicitNonRange> vec2 = INIT(INIT(1,2), INIT(3,4), INIT(ExplicitNonRange(1,2)));
        ASSERT_EQ(vec2.size(), 3);

        // Range element.
        std::vector<ExplicitRange> vec3 = INIT(INIT(1,2), INIT(3,4), INIT(5,6));
        ASSERT_EQ(vec3.size(), 3);
        // Range element, heterogeneous list.
        std::vector<ExplicitRange> vec4 = INIT(INIT(1,2), INIT(3,4), INIT(5,6,7));
        ASSERT_EQ(vec4.size(), 3);
    }

    { // Copyability of lists, and ref-qualifiers of conversion operators.
        int x = 42;
        float y = 43;

        using HomogeneousLvalue = decltype(INIT(x, x, x));
        using HomogeneousLvalueExtra = decltype(INIT(x, x, x).and_with(42, 43, 44));
        // Copyability.
        static_assert(std::is_copy_constructible<HomogeneousLvalue>::value && std::is_copy_assignable<HomogeneousLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_convertible<HomogeneousLvalue &, ExplicitRange>::value && std::is_constructible<ExplicitRange, HomogeneousLvalue &>::value, "");
        static_assert(std::is_convertible<HomogeneousLvalue &, ImplicitRange>::value && std::is_constructible<ImplicitRange, HomogeneousLvalue &>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_convertible<HomogeneousLvalueExtra &, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HomogeneousLvalueExtra &>::value, "");
        static_assert(std::is_convertible<HomogeneousLvalueExtra &, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HomogeneousLvalueExtra &>::value, "");

        using HeterogeneousLvalue = decltype(INIT(x, x, y));
        using HeterogeneousLvalueExtra = decltype(INIT(x, x, y).and_with(42, 43, 44));
        // Copyability.
        static_assert(std::is_copy_constructible<HeterogeneousLvalue>::value && std::is_copy_assignable<HeterogeneousLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_convertible<HeterogeneousLvalue &, ExplicitRange>::value && std::is_constructible<ExplicitRange, HeterogeneousLvalue &>::value, "");
        static_assert(std::is_convertible<HeterogeneousLvalue &, ImplicitRange>::value && std::is_constructible<ImplicitRange, HeterogeneousLvalue &>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_convertible<HeterogeneousLvalueExtra &, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HeterogeneousLvalueExtra &>::value, "");
        static_assert(std::is_convertible<HeterogeneousLvalueExtra &, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HeterogeneousLvalueExtra &>::value, "");

        using HomogeneousNonLvalue = decltype(INIT(42, x, x));
        using HomogeneousNonLvalueExtra = decltype(INIT(42, x, x).and_with(42, 43, 44));
        // Copyability.
        static_assert(!std::is_copy_constructible<HomogeneousNonLvalue>::value && !std::is_copy_assignable<HomogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRange, HomogeneousNonLvalue &>::value && !std::is_convertible<HomogeneousNonLvalue, ExplicitRange>::value && std::is_constructible<ExplicitRange, HomogeneousNonLvalue>::value, "");
        static_assert(!std::is_constructible<ImplicitRange, HomogeneousNonLvalue &>::value && std::is_convertible<HomogeneousNonLvalue, ImplicitRange>::value && std::is_constructible<ImplicitRange, HomogeneousNonLvalue>::value, "");
        // Ref-qualifiers on conversions with extra args, explicit and implicit.
        static_assert(!std::is_constructible<ExplicitRangeWithArgs, HomogeneousNonLvalueExtra &>::value && !std::is_convertible<HomogeneousNonLvalueExtra, ExplicitRangeWithArgs>::value && std::is_constructible<ExplicitRangeWithArgs, HomogeneousNonLvalueExtra>::value, "");
        static_assert(!std::is_constructible<ImplicitRangeWithArgs, HomogeneousNonLvalueExtra &>::value && std::is_convertible<HomogeneousNonLvalueExtra, ImplicitRangeWithArgs>::value && std::is_constructible<ImplicitRangeWithArgs, HomogeneousNonLvalueExtra>::value, "");

        using HeterogeneousNonLvalue = decltype(INIT(42, x, y));
        using HeterogeneousNonLvalueExtra = decltype(INIT(42, x, y).and_with(42, 43, 44));
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
        static_assert(HasBeginEnd<decltype(INIT(x, y, z))>::value, "");
        static_assert(HasBeginEnd<decltype(INIT(x, y, z)) &>::value, "");
        // Non-lvalue-only lists have rvalue-ref-qualified begin/end.
        static_assert(HasBeginEnd<decltype(INIT(1, 2, 3))>::value, "");
        static_assert(!HasBeginEnd<decltype(INIT(1, 2, 3)) &>::value, "");
        // Heterogeneous lists don't have begin/end.
        static_assert(!HasBeginEnd<decltype(INIT(x, 2, 3))>::value, "");
        static_assert(!HasBeginEnd<decltype(INIT(x, 2, 3)) &>::value, "");

        // Check that `std::sort` likes our iterators.
        auto i = INIT(x, y, z);
        std::sort(i.begin(), i.end());
        ASSERT_EQ(x, 1);
        ASSERT_EQ(y, 2);
        ASSERT_EQ(z, 3);
    }

    { // `list(list)` deduction.
        // It's not entirely clear why this fails in C++14.
        // It boils down to `std::vector<int> v(INIT())` not compiling, as opposed to `std::vector<int> v = INIT()`, which does compile.
        // But why is a mystery.
        #if BETTERLISTINIT_CXX_STANDARD > 14
        std::vector<std::vector<int>> v = INIT(INIT());
        ASSERT_EQ(v.size(), 1);
        ASSERT_EQ(v[0].size(), 0);
        #endif
    }

    std::cout << "OK";
    #if BETTERLISTINIT_CXX_STANDARD >= 17 && !CONTAINERS_HAVE_MANDATORY_COPY_ELISION
    std::cout << " (without mandatory copy elision)";
    #endif
    #if BETTERLISTINIT_ALLOCATOR_HACK
    std::cout << " (with" << (better_list_init::detail::allocator_hack::enabled::value ? "" : " inactive") << " allocator hack)";
    #endif
    std::cout << "\n";
}
