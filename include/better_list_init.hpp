#pragma once

// ~ better_list_init ~
// Provides a way to initialize containers of non-movable types, since `std::initializer_list` doesn't move anything.
// Minimal usage example: `std::vector<T> foo = init{...};` instead of `= {...}`.
// See the readme for more details.

// The latest version is hosted at: https://github.com/HolyBlackCat/better_list_init

// License: ZLIB

// Copyright (c) 2022-2024 Egor Mikhailov
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include <initializer_list>
#include <type_traits>

// The version number: `major*10000 + minor*100 + patch`.
#ifndef BETTERLISTINIT_VERSION
#define BETTERLISTINIT_VERSION 10001
#endif

// This file is included by this header automatically, if it exists.
// Put your customizations here.
// You can redefine various macros defined below (in the config file, or elsewhere),
// and specialize templates in `better_list_init::custom` (also in the config file, or elsewhere).
#ifndef BETTERLISTINIT_CONFIG
#define BETTERLISTINIT_CONFIG "better_list_init_config.hpp"
#endif

// CONTAINER REQUIREMENTS
// All of those can be worked around by specializing `better_list_init::custom::??` for your container.
// * Must have a `::value_type` typedef with the element type.
// * Must have a constructor from two iterators (possibly followed by other arguments, then you have to use `init{...}.and_with(extra_args...)`).

namespace better_list_init
{
    namespace detail
    {
        // Convertible to any `std::initializer_list<??>`.
        struct any_init_list
        {
            template <typename T>
            operator std::initializer_list<T>() const noexcept; // Not defined.
        };

        // Don't want to include extra headers, so I roll my own typedefs.
        using nullptr_t = decltype(nullptr);
        using size_t = decltype(sizeof(int));
        using ptrdiff_t = decltype((int *)nullptr - (int *)nullptr);
        using uintptr_t = size_t;
        static_assert(sizeof(uintptr_t) == sizeof(void *), "Internal error: Incorrect `uintptr_t` typedef, fix it.");

        template <typename T>
        T &&declval() noexcept; // Not defined.

        template <typename T>
        void accept_parameter(T) noexcept; // Not defined.

        // Whether `T` is implicitly constructible from a braced list of `P...`.
        template <typename Void, typename T, typename ...P>
        struct implicitly_brace_constructible_helper : std::false_type {};
        template <typename T, typename ...P>
        struct implicitly_brace_constructible_helper<decltype(accept_parameter<T &&>({declval<P>()...})), T, P...> : std::true_type {};
        template <typename T, typename ...P>
        struct implicitly_brace_constructible : implicitly_brace_constructible_helper<void, T, P...> {};

        template <typename T, typename ...P>
        struct nothrow_implicitly_brace_constructible : std::integral_constant<bool, noexcept(accept_parameter<T &&>({declval<P>()...}))> {};

        // The default implementation for `custom::element_type`, see below.
        template <typename T, typename = void>
        struct default_element_type {};
        template <typename T>
        struct default_element_type<T, decltype(void(declval<typename T::value_type>))> {using type = typename T::value_type;};

        // The default implementation for `custom::construct_range`, see below.
        template <typename Void, typename T, typename Iter, typename List, typename ...P>
        struct default_construct_range
        {
            template <typename TT = T, std::enable_if_t<std::is_constructible<TT, Iter, Iter, P...>::value, detail::nullptr_t> = nullptr>
            constexpr T operator()(Iter begin, Iter end, P &&... params) const noexcept(std::is_nothrow_constructible<T, Iter, Iter, P...>::value)
            {
                // Don't want to include `<utility>` for `std::move` or `std::forward`.
                return T(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...);
            }
        };

        // The default implementation for `custom::construct_nonrange`, see below.
        template <typename Void, typename T, typename List, typename ...P>
        struct default_construct_nonrange
        {
            // Note `TT = T`. The SFINAE condition has to depend on this function's template parameters, otherwise this is a hard error.
            template <typename TT = T>
            constexpr auto operator()(P &&... params) const noexcept(noexcept(TT{static_cast<P &&>(params)...})) -> decltype(TT{static_cast<P &&>(params)...})
            {
                return T{static_cast<P &&>(params)...};
            }
        };
    }

    // Customization points.
    namespace custom
    {
        // Whether to make the conversion operator of `init{...}` implicit. `T` is the target container type,
        // and `P...` are the extra constructor arguments.
        // Defaults to checking if `T` has a `std::initializer_list<U>` constructor, for any `U`.
        template <typename Void, typename T, typename ...P>
        struct allow_implicit_range_init : std::is_constructible<T, detail::any_init_list, P...> {};
        // Same, but for braced init (for non-ranges), as opposed to initializing ranges from a pair of iterators.
        // Defaults to checking if `T` is implicitly constructible from a braced list of `P...`.
        // NOTE: Here the meaning of `P...` is different compared to the `allow_implicit_range_init`.
        template <typename Void, typename T, typename ...P>
        struct allow_implicit_nonrange_init : detail::implicitly_brace_constructible<T, P...> {};

        // Because of a MSVC quirk (bug, probably?) we can't use a templated `operator T` for our range elements,
        // and must know exactly what we're converting to.
        // By default, return `T::value_type`, if any.
        template <typename T, typename = void>
        struct element_type : detail::default_element_type<T> {};

        // How to construct `T` from a pair of iterators. Defaults to `T(begin, end, extra...)`,
        // where `extra...` are the arguments passed to `.and_with(...)`, if any.
        // `List` receives the type of the used `init` class.
        template <typename Void, typename T, typename Iter, typename List, typename ...P>
        struct construct_range : detail::default_construct_range<void, T, Iter, List, P...> {};

        // How to construct `T` (which is not a range) from a braced list. Defaults to `T{P...}`, where `P...` are the list elements.
        // `List` receives the type of the used `init` class.
        template <typename Void, typename T, typename List, typename ...P>
        struct construct_nonrange : detail::default_construct_nonrange<void, T, List, P...> {};
    }

    namespace detail
    {
        // Normally matches `std::is_aggregate`, but since it doesn't exist in C++14, in that case we consider `std::array` to be the only aggregate.
        // Specialize this template or `custom::is_range` if this is problematic.
        // This is specialized after including the config, because the specializations depend on the C++ version.
        template <typename T, typename = void>
        struct is_aggregate : std::false_type {};

        // The default value for `custom::is_range`, see below.
        template <typename T, typename = void>
        struct default_is_range : std::false_type {};
        template <typename T>
        struct default_is_range<T, decltype(void(declval<typename custom::element_type<T>::type>()))> : std::integral_constant<bool, !is_aggregate<T>::value> {};
    }

    // More customization points.
    namespace custom
    {
        // Whether `T` looks like a container or not.
        // If yes, we're going to try to construct it from a pair of iterators. If not, we're going to try to initialize it with a braced list.
        // We don't want to attempt both, because that looks error-prone (a-la the uniform init fiasco).
        // By default, we're looking for `custom::element_type` (which defaults to `::value_type`), but reject aggregates.
        template <typename T, typename = void>
        struct is_range : detail::default_is_range<T> {};
    }
}

// Include the user config, if it exists.
#if __has_include(BETTERLISTINIT_CONFIG)
#include BETTERLISTINIT_CONFIG
#endif

// Whether to add `init` in the global namespace as a shorthand for `better_list_init::init{...}`.
#ifndef BETTERLISTINIT_SHORTHAND
#define BETTERLISTINIT_SHORTHAND 1
#endif

// Lets you replace the name `init` with something else.
#ifndef BETTERLISTINIT_IDENTIFIER
#define BETTERLISTINIT_IDENTIFIER init
#endif

// The C++ standard version to assume.
// First, the raw date number.
#ifndef BETTERLISTINIT_CXX_STANDARD_DATE
#ifdef _MSC_VER
#define BETTERLISTINIT_CXX_STANDARD_DATE _MSVC_LANG // D:<
#else
#define BETTERLISTINIT_CXX_STANDARD_DATE __cplusplus
#endif
#endif
// Then, the actual version number.
#ifndef BETTERLISTINIT_CXX_STANDARD
#if BETTERLISTINIT_CXX_STANDARD_DATE >= 202002
#define BETTERLISTINIT_CXX_STANDARD 20
#elif BETTERLISTINIT_CXX_STANDARD_DATE >= 201703
#define BETTERLISTINIT_CXX_STANDARD 17
#elif BETTERLISTINIT_CXX_STANDARD_DATE >= 201402
#define BETTERLISTINIT_CXX_STANDARD 14
// #elif BETTERLISTINIT_CXX_STANDARD_DATE >= 201103
// #define BETTERLISTINIT_CXX_STANDARD 11
#else
#error "better_list_init requires C++14 or newer."
#endif
#endif

// Whether to allow braces: `init{...}`. Parentheses are always allowed: `init(...)`.
// Braces require CTAD to work.
// Note that here and elsewhere in C++, elements in braces are evaluated left-to-right, while in parentheses the evaluation order is unspecified.
#ifndef BETTERLISTINIT_ALLOW_BRACES
#if BETTERLISTINIT_CXX_STANDARD >= 17
#define BETTERLISTINIT_ALLOW_BRACES 1
#else
#define BETTERLISTINIT_ALLOW_BRACES 0
#endif
#endif

// Is the cast to/from `void *` constexpr?
#ifndef BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST
#if __cpp_constexpr >= 202306
#define BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST 1
#else
#define BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST 0
#endif
#endif
// `constexpr` if the `void *` cast itself is constexpr.
#if BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST
#define BETTERLISTINIT_CONSTEXPR_VOID_PTR_CAST constexpr
#else
#define BETTERLISTINIT_CONSTEXPR_VOID_PTR_CAST
#endif

// Should we use a simplified tuple implementation for heterogeneous lists?
// This compiles faster, but isn't constexpr until C++26.
// Enabled by default only if can be made constexpr.
#ifndef BETTERLISTINIT_USE_VOID_PTR_ARRAY_AS_TUPLE
#if BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST
#define BETTERLISTINIT_USE_VOID_PTR_ARRAY_AS_TUPLE 1
#else
#define BETTERLISTINIT_USE_VOID_PTR_ARRAY_AS_TUPLE 0
#endif
#endif

// We want avoid including the entire `<iterator>` just to get the iterator tag.
// And we can't use C++20 iterator category detection, because the detection logic is poorly specified to not consider iterators with `::reference` being an rvalue reference.
// This is an LWG issue: https://cplusplus.github.io/LWG/issue3798
// If this is enabled, we try to forward-declare `std::random_access_iterator_tag` in a way suitable for the current standard library, falling back to including `<iterator>`.
// If this is disabled, we just include `<iterator>`.
#ifndef BETTERLISTINIT_FORWARD_DECLARE_ITERATOR_TAG
#define BETTERLISTINIT_FORWARD_DECLARE_ITERATOR_TAG 1
#endif

// How to stop the program when something bad happens.
// This statement will be wrapped in 'diagostic push/pop' automatically.
#ifndef BETTERLISTINIT_ABORT
#if defined(_MSC_VER) && defined(__clang__)
#define BETTERLISTINIT_ABORT _Pragma("clang diagnostic ignored \"-Winvalid-noreturn\"") __debugbreak();
#elif defined(_MSC_VER)
#define BETTERLISTINIT_ABORT __debugbreak();
#else
#define BETTERLISTINIT_ABORT __builtin_trap();
#endif
#endif

// `[[nodiscard]]`, if available.
#ifndef BETTERLISTINIT_NODISCARD
#if BETTERLISTINIT_CXX_STANDARD >= 17
#define BETTERLISTINIT_NODISCARD [[nodiscard]]
#else
#define BETTERLISTINIT_NODISCARD
#endif
#endif

// Whether `std::is_aggregate` is available.
#ifndef BETTERLISTINIT_HAVE_IS_AGGREGATE
#if BETTERLISTINIT_CXX_STANDARD >= 17
#define BETTERLISTINIT_HAVE_IS_AGGREGATE 1
#else
#define BETTERLISTINIT_HAVE_IS_AGGREGATE 0
#endif
#endif

// Whether to allow the 'allocator hack', which means creating the container with a modified allocator, then converting it to the original allocator.
// The modified allocator doesn't rely on copy elision, and constructs the object directly at the correct location.
// We use it only in MSVC, in C++20 mode, to work around a bug that causes `std::construct_at` (used by `std::vector` and others) to reject initialization that utilizes the mandatory copy elision.
// This is issue https://github.com/microsoft/STL/issues/2620, fixed at June 20, 2022 by commit https://github.com/microsoft/STL/blob/3f203cb0d9bfde929a75eed877c228f88c0c7a46/stl/inc/xutility.
// In theory, it could also help with the lack of mandatory copy elision pre-C++17, but it doesn't work on libc++ in C++14, because it uses a strict SFINAE e.g. on vector's constructor,
// which doesn't respect allocator's `construct()`.
// But you could use libstdc++ in C++14 mode to test the allocator hack on compilers other than MSVC.
// By default, even if this is enabled, the hack is disabled at compile-time if we detect that your standard library is not bugged.
// Set `BETTERLISTINIT_ALLOCATOR_HACK` to 2 to enable it unconditionally, for testing.
#ifndef BETTERLISTINIT_ALLOCATOR_HACK
#if defined(_MSC_VER) && _MSC_VER < 1934 && BETTERLISTINIT_CXX_STANDARD >= 20
#define BETTERLISTINIT_ALLOCATOR_HACK 1
#else
#define BETTERLISTINIT_ALLOCATOR_HACK 0
#endif
#endif

// When the allocator hack is used, we need a 'may alias' attribute to `reinterpret_cast` safely.
#ifndef BETTERLISTINIT_ALLOCATOR_HACK_MAY_ALIAS
#ifdef _MSC_VER
#define BETTERLISTINIT_ALLOCATOR_HACK_MAY_ALIAS // I've heard MSVC is fairly conservative with aliasing optimizations?
#else
#define BETTERLISTINIT_ALLOCATOR_HACK_MAY_ALIAS __attribute__((__may_alias__))
#endif
#endif

// If true, we can replace an allocator's `.construct()` func with placement-new, if we have to.
// Normally this is not useful, because:
// A. C++20 `std::allocator` doesn't define `.construct()`, and
// B. If you have a lame allocator that does define it without a good reason, you can specialize a few of our templates for it.
#ifndef BETTERLISTINIT_ALLOCATOR_HACK_IGNORE_EXISTING_CONSTRUCT_FUNC
#define BETTERLISTINIT_ALLOCATOR_HACK_IGNORE_EXISTING_CONSTRUCT_FUNC 0
#endif


#if !BETTERLISTINIT_FORWARD_DECLARE_ITERATOR_TAG
#include <iterator>
#else
#if defined(__GLIBCXX__)
namespace std _GLIBCXX_VISIBILITY(default)
{
    _GLIBCXX_BEGIN_NAMESPACE_VERSION
    struct random_access_iterator_tag;
    _GLIBCXX_END_NAMESPACE_VERSION
}
#elif defined(_LIBCPP_VERSION)
_LIBCPP_BEGIN_NAMESPACE_STD
struct _LIBCPP_TEMPLATE_VIS random_access_iterator_tag;
_LIBCPP_END_NAMESPACE_STD
#elif defined(_MSC_VER)
_STD_BEGIN
struct random_access_iterator_tag;
_STD_END
#else
#include <iterator>
#endif
#endif

#if BETTERLISTINIT_ALLOCATOR_HACK
#include <memory> // For `std::allocator_traits`.
#endif

#if !BETTERLISTINIT_HAVE_IS_AGGREGATE
#include <array> // Need this to specialize `detail::default_is_range`, see below for details.
#endif

namespace better_list_init
{
    namespace detail
    {
        #if BETTERLISTINIT_HAVE_IS_AGGREGATE
        template <typename T>
        struct is_aggregate<T, std::enable_if_t<std::is_aggregate<T>::value>> : std::true_type {};
        #else
        // No `std::is_aggregate` in C++14, so we fall back to assuming that only `std::array` is an aggregate.
        // If this is problematic, specialize either this template or `custom::is_range` for your type.
        template <typename T, size_t N>
        struct is_aggregate<std::array<T, N>> : std::true_type {};
        #endif


        struct empty {};

        // In the definition of `BETTERLISTINIT_ABORT`, we promise to push/pop diagnostics around it.
        #ifdef _MSC_VER
        #pragma warning(push)
        #else
        #pragma GCC diagnostic push
        #endif
        [[noreturn]] inline void abort() {BETTERLISTINIT_ABORT}
        #ifdef _MSC_VER
        #pragma warning(pop)
        #else
        #pragma GCC diagnostic pop
        #endif

        // A boolean, artifically dependent on a type.
        template <typename T, bool X>
        struct dependent_value : std::integral_constant<bool, X> {};

        // Not using fold expressions nor `std::{con,dis}junction` to avoid depending on C++17.
        // Those mimic the latter.
        template <typename ...P> struct all_of : std::true_type {};
        template <typename T> struct all_of<T> : T {};
        template <typename T, typename ...P> struct all_of<T, P...> : std::conditional_t<T::value, all_of<P...>, T> {};

        template <typename ...P> struct any_of : std::false_type {};
        template <typename T> struct any_of<T> : T {};
        template <typename T, typename ...P> struct any_of<T, P...> : std::conditional_t<T::value, T, any_of<P...>> {};

        template <typename T> struct negate : std::integral_constant<bool, !T::value> {};

        // Returns true if all types in `P...` are the same.
        template <typename ...P>
        struct all_types_same : std::true_type {};
        template <typename T, typename ...P>
        struct all_types_same<T, P...> : all_of<std::is_same<T, P>...> {};

        // Returns the first type in a list.
        template <typename ...P>
        struct first_type {};
        template <typename T, typename ...P>
        struct first_type<T, P...> {using type = T;};

        // An empty struct, copyable only if the condition is true.
        template <bool IsCopyable>
        struct maybe_copyable {};
        template <>
        struct maybe_copyable<false>
        {
            maybe_copyable() = default;
            maybe_copyable(const maybe_copyable &) = delete;
            maybe_copyable &operator=(const maybe_copyable &) = delete;
        };

        // Use `deduce...` to force the following template parameters to be deduced, as opposed to being specified manually.
        struct deduce_helper
        {
          protected:
            deduce_helper() = default;
        };
        using deduce = deduce_helper &;

        // We use a custom tuple class to avoid including `<tuple>` and because we need some extra functionality.

        #if !BETTERLISTINIT_TUPLE_IMPL_V2
        // A helper class for our tuple implementation.
        template <typename ...P>
        struct tuple_impl_regular_low
        {
            // See below.
            template <typename F, typename ...Q>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return static_cast<F &&>(func)(static_cast<Q &&>(params)...);
            }
        };
        template <typename T>
        struct tuple_impl_regular_low<T>
        {
            using first_t = std::remove_reference_t<T> *;
            first_t first;

            constexpr tuple_impl_regular_low(first_t first) : first(first) {}

            // Returns an element type by its index.
            template <size_t I>
            using elem_t = T;

            // Returns an element by its index.
            template <size_t I>
            constexpr first_t get() const
            {
                static_assert(I == 0, "Internal error: The tuple index is out of range.");
                return first;
            }

            // Fills the array with the instances of `F::func<I>`.
            template <typename F, size_t I = 0, typename E>
            static constexpr void populate_func_array(E *array)
            {
                *array = F::template func<I>;
            }

            // Calls the function with the specified parameters, followed by the tuple elements.
            // The extra parameters are normally the preceding tuple elements.
            template <typename F, typename ...Q>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return static_cast<F &&>(func)(static_cast<Q &&>(params)..., static_cast<T &&>(*first));
            }
        };
        template <typename T, typename ...P>
        struct tuple_impl_regular_low<T, P...>
        {
            using first_t = std::remove_reference_t<T> *;
            first_t first;

            using next_t = tuple_impl_regular_low<P...>;
            next_t next;

            constexpr tuple_impl_regular_low(first_t first, std::remove_reference_t<P> *... next) : first(first), next(next...) {}

            template <size_t I>
            using elem_t = std::conditional_t<I == 0, T, typename next_t::template elem_t<I-1>>;

            template <size_t I, std::enable_if_t<I == 0, nullptr_t> = nullptr>
            constexpr first_t get() const
            {
                return first;
            }
            template <size_t I, std::enable_if_t<(I > 0), nullptr_t> = nullptr>
            constexpr decltype(next.template get<I-1>()) get() const
            {
                return next.template get<I-1>();
            }

            template <typename F, size_t I = 0, typename E>
            static constexpr void populate_func_array(E *array)
            {
                *array = F::template func<I>;
                next_t::template populate_func_array<F, I+1>(array + 1);
            }

            template <typename F, typename ...Q>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return next.apply(static_cast<F &&>(func), static_cast<Q &&>(params)..., static_cast<T &&>(*first));
            }
        };
        // The tuple implementation itself.
        template <typename ...P>
        class tuple_impl_regular : tuple_impl_regular_low<P...>
        {
            using base_t = tuple_impl_regular_low<P...>;

            // Various helpers for `.apply()` defined below.

            template <typename F, typename ...Q>
            struct make_elem_func
            {
                template <size_t I>
                static constexpr typename F::return_type func(const base_t &base, Q &&... params)
                {
                    return F::template func<typename base_t::template elem_t<I>>(*base.template get<I>(), static_cast<Q &&>(params)...);
                }
            };

            template <typename F, typename ...Q>
            struct elem_func_array {typename F::return_type (*funcs[sizeof...(P)])(const base_t &, Q &&...){};};

            template <typename F, typename ...Q>
            static constexpr elem_func_array<F, Q &&...> make_elem_func_array()
            {
                elem_func_array<F, Q &&...> ret;
                base_t::template populate_func_array<make_elem_func<F, Q &&...>>(ret.funcs);
                return ret;
            }

          public:
            using base_t::base_t;

            // Applies a custom function to the `i`th element.
            // `F` must have `::return_type` and `::func<T>(T &)`, where `T` receives the element type, and can be an rvalue reference.
            template <typename F, typename ...Q, std::enable_if_t<dependent_value<F, sizeof...(P) != 0>::value, nullptr_t> = nullptr>
            constexpr typename F::return_type apply_to_elem(size_t i, Q &&... params) const
            {
                constexpr elem_func_array<F, Q &&...> array = make_elem_func_array<F, Q &&...>();
                return array.funcs[i](*this, static_cast<Q &&>(params)...);
            }
            template <typename F, typename ...Q, std::enable_if_t<dependent_value<F, sizeof...(P) == 0>::value, nullptr_t> = nullptr>
            constexpr typename F::return_type apply_to_elem(size_t, Q &&...) const
            {
                abort();
            }

            // Applies a custom function to all the elements at once. `params...` are prepended to the tuple elements.
            using base_t::apply; // decltype(auto) apply(F &&func, Q &&... params);
        };
        #else // if BETTERLISTINIT_TUPLE_IMPL_V2
        // The tuple implementation. This one needs constexpr `void *` cast to be constexpr.
        template <typename ...P>
        class tuple_impl_regular
        {
          public:
            // We want to keep this an aggregate, for simplicity.
            const void *values[sizeof...(P)];

          private:
            // Applies a custom function to all the elements at once. `params...` are prepended to the tuple elements.
            template <size_t I, typename R0, typename ...R, typename F, typename ...Q, std::enable_if_t<I == sizeof...(P)-1, nullptr_t> = nullptr>
            constexpr decltype(auto) apply_low(F &&func, Q &&... params) const
            {
                return static_cast<F &&>(func)(static_cast<Q &&>(params)..., static_cast<R0 &&>(*static_cast<std::remove_reference_t<R0> *>(const_cast<void *>(values[I]))));
            }
            template <size_t I, typename R0, typename ...R, typename F, typename ...Q, std::enable_if_t<I != sizeof...(P)-1, nullptr_t> = nullptr>
            constexpr decltype(auto) apply_low(F &&func, Q &&... params) const
            {
                return apply_low<I+1, R...>(static_cast<F &&>(func), static_cast<Q &&>(params)..., static_cast<R0 &&>(*static_cast<std::remove_reference_t<R0> *>(const_cast<void *>(values[I]))));
            }

          public:
            // Applies a custom function to the `i`th element.
            // `F` must have `::return_type` and `::func<T>(T &)`, where `T` receives the element type, and can be an rvalue reference.
            template <typename F, typename ...Q, std::enable_if_t<dependent_value<F, sizeof...(P) != 0>::value, nullptr_t> = nullptr>
            constexpr typename F::return_type apply_to_elem(size_t i, Q &&... params) const
            {
                constexpr typename F::return_type (*array[])(const void *value, Q &&... params) = {
                    +[](const void *value, Q &&... params) -> typename F::return_type
                    {
                        return F::template func<P>(*static_cast<std::remove_reference_t<P> *>(const_cast<void *>(value)), static_cast<Q &&>(params)...);
                    }...
                };
                return array[i](values[i], static_cast<Q &&>(params)...);
            }
            template <typename F, typename ...Q, std::enable_if_t<dependent_value<F, sizeof...(P) == 0>::value, nullptr_t> = nullptr>
            constexpr typename F::return_type apply_to_elem(size_t, Q &&...) const
            {
                abort();
            }

            template <typename F, typename ...Q>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return apply_low<0, P...>(static_cast<F &&>(func), static_cast<Q &&>(params)...);
            }
        };

        template <>
        class tuple_impl_regular<>
        {
          public:
            template <typename F, typename ...Q>
            constexpr typename F::return_type apply_to_elem(size_t, Q &&...) const
            {
                abort();
            }

            template <typename F, typename ...Q>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return static_cast<F &&>(func)(static_cast<Q &&>(params)...);
            }
        };
        #endif

        // A simple alternative array-based tuple implementation, for homogeneous non-empty tuples.
        template <typename T, size_t N>
        class tuple_impl_array
        {
          public:
            // We want to keep this an aggregate, for simplicity.
            std::remove_reference_t<T> *values[N];

            template <typename F, typename ...Q>
            constexpr typename F::return_type apply_to_elem(size_t i, Q &&... params) const
            {
                return F::template func<T>(*values[i], static_cast<Q &&>(params)...);
            }

            template <size_t I = 0, typename F, typename ...Q, std::enable_if_t<I == N-1, nullptr_t> = nullptr>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return static_cast<F &&>(func)(static_cast<Q &&>(params)..., static_cast<T &&>(*values[I]));
            }
            template <size_t I = 0, typename F, typename ...Q, std::enable_if_t<I != N-1, nullptr_t> = nullptr>
            constexpr decltype(auto) apply(F &&func, Q &&... params) const
            {
                return apply<I+1>(static_cast<F &&>(func), static_cast<Q &&>(params)..., static_cast<T &&>(*values[I]));
            }
        };

        // Selects an implementation for our tuple. If all element types are the same (and there is at least one),
        // a simplified array-based implementation is used.
        template <typename Void, typename ...P>
        struct tuple_impl_selector
        {
            using type = tuple_impl_regular<P...>;
        };
        template <typename ...P>
        struct tuple_impl_selector<std::enable_if_t<all_types_same<P...>::value && sizeof...(P) != 0>, P...>
        {
            using type = tuple_impl_array<typename first_type<P...>::type, sizeof...(P)>;
        };

        // Our custom tuple.
        template <typename ...P>
        using tuple = typename tuple_impl_selector<void, P...>::type;

        // Calls `.apply()` on a tuple.
        template <typename F, typename T>
        struct apply_functor
        {
            F &&func;
            T &&tuple;

            template <typename ...P>
            constexpr decltype(auto) operator()(P &&... params) const
            {
                return static_cast<T &&>(tuple).apply(static_cast<F &&>(func), static_cast<P &&>(params)...);
            }
        };


        template <typename T>
        struct construct_from_elem
        {
            using return_type = T;
            template <typename U>
            static constexpr T func(U &source)
            {
                return T(static_cast<U &&>(source));
            }
        };

        #if BETTERLISTINIT_ALLOCATOR_HACK
        namespace allocator_hack
        {
            // Constructs a `T` at `target` using allocator `A`, passing a forwarding reference to `U` as an argument.
            template <typename T, typename A>
            struct construct_from_elem_at
            {
                using return_type = void;
                template <typename U>
                static constexpr void func(U &source, A &alloc, T *target)
                {
                    std::allocator_traits<A>::template construct(alloc, target, static_cast<U &&>(source));
                }
            };
        }
        #endif

        // Our iterator's value type inherits from this.
        struct elem_ref_base {};

        // Replaces `std::is_constructible`. The standard trait is buggy at least in MSVC v19.32.
        template <typename Void, typename T, typename ...P>
        struct constructible_helper : std::false_type {};
        template <typename T, typename ...P>
        struct constructible_helper<decltype(void(T(std::declval<P>()...))), T, P...> : std::true_type {};
        template <typename T, typename ...P>
        struct constructible : constructible_helper<void, T, P...> {};

        template <typename T, typename ...P>
        struct nothrow_constructible : std::integral_constant<bool, noexcept(T(declval<P>()...))> {};

        // Whether `T` is constructible from a pair of `Iter`s, possibly with extra arguments.
        // `List` is the `init<...>` specialization performing the conversion.
        template <typename Void, typename T, typename Iter, typename List, typename ...P>
        struct constructible_from_iters_helper : std::false_type {};
        template <typename T, typename Iter, typename List, typename ...P>
        struct constructible_from_iters_helper<decltype(void(custom::construct_range<void, T, Iter, List, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))), T, Iter, List, P...> : std::true_type {};
        template <typename T, typename Iter, typename List, typename ...P>
        struct constructible_from_iters : constructible_from_iters_helper<void, T, Iter, List, P...> {};

        template <typename T, typename Iter, typename List, typename ...P>
        struct nothrow_constructible_from_iters : std::integral_constant<bool, noexcept(custom::construct_range<void, T, Iter, List, P...>{}(declval<Iter &&>(), declval<Iter &&>(), declval<P &&>()...))> {};

        // Whether `T` is constructible from a braced list of `P...`.
        // `List` is the `init<...>` specialization performing the conversion.
        template <typename Void, typename T, typename List, typename ...P>
        struct nonrange_brace_constructible_helper : std::false_type {};
        template <typename T, typename List, typename ...P>
        struct nonrange_brace_constructible_helper<decltype(void(custom::construct_nonrange<void, T, List, P...>{}(declval<P>()...))), T, List, P...> : std::true_type {};
        template <typename T, typename List, typename ...P>
        struct nonrange_brace_constructible : nonrange_brace_constructible_helper<void, T, List, P...> {};

        template <typename T, typename List, typename ...P>
        struct nothrow_nonrange_brace_constructible : std::integral_constant<bool, noexcept(custom::construct_nonrange<void, T, List, P...>{}(declval<P>()...))> {};

        // Whether `T` is a valid target type for any conversion operator. We reject const types here.
        // Normally this is not an issue, but GCC 10 in C++14 mode has quirky tendency to try to instantiate conversion operators to const types otherwise (for copy constructors?),
        // which breaks when the types are non-copyable in a SFINAE-unfriendly way.
        template <typename T>
        using enable_if_valid_conversion_target = std::enable_if_t<!std::is_const<T>::value, int>;
    }

    // `better_list_init::type::init` is the type of our list class.
    // I don't want to put it into `detail`, because it can be renamed by the user, and I don't want naming conflicts.
    namespace type
    {
        template <typename ...P>
        class BETTERLISTINIT_NODISCARD BETTERLISTINIT_IDENTIFIER
            : detail::maybe_copyable<detail::all_of<std::is_lvalue_reference<P>...>::value>
        {
          public:
            // Whether this list can be used to initialize a range of `T`s.
            template <typename T> struct can_initialize_elem         : detail::all_of<detail::constructible        <T, P>...> {};
            template <typename T> struct can_nothrow_initialize_elem : detail::all_of<detail::nothrow_constructible<T, P>...> {};

            // Whether all types in `P...` are the same (and there is at least one type). Then we can simplify some logic.
            // static constexpr bool is_homogeneous = detail::all_types_same<P...>::value && sizeof...(P) > 0;
            static constexpr bool is_homogeneous = detail::all_types_same<P...>::value && sizeof...(P) > 0;
            // If all types in `P...` are the same (and there's at least one), returns that type. Otherwise returns an empty struct.
            using homogeneous_type = typename std::conditional_t<is_homogeneous, detail::first_type<P &&...>, std::enable_if<true, detail::empty>>::type;

            // Whether all our references are lvalue references. Such lists can be copied.
            static constexpr bool is_lvalue_only = detail::all_of<std::is_lvalue_reference<P>...>::value;

          private:
            using tuple_t = detail::tuple<P &&...>;

            // Heterogeneous lists use this as the element type for the iterators.
            template <typename T>
            class elem_ref : detail::elem_ref_base
            {
                friend BETTERLISTINIT_IDENTIFIER;
                const tuple_t *target = nullptr;
                detail::size_t index = 0;

                constexpr elem_ref() {}

              public:
                // Non-copyable.
                // The list creates and owns all its references, and exposes actual references to them.
                // This is because pre-C++20 iterator requirements force us to return actual references from `*`, and more importantly `[]`.
                elem_ref(const elem_ref &) = delete;
                elem_ref &operator=(const elem_ref &) = delete;

                // MSVC is bugged and refuses to let us default-construct references in some scenarios, despite `init` being our `friend`.
                // We work around this by creating arrays here.
                template <detail::size_t N>
                struct array
                {
                    elem_ref elems[N];
                    constexpr array() {}
                };

                // Note that this is unnecessary when `is_homogeneous` is true, because in that case our iterator
                // dereferences directly to `homogeneous_type`.
                constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>::value)
                {
                    return target->template apply_to_elem<detail::construct_from_elem<T>>(index);
                }

                #if BETTERLISTINIT_ALLOCATOR_HACK
                // Constructs an object at the specified address, using an allocator.
                template <typename Alloc>
                constexpr void _allocator_hack_construct_at(Alloc &alloc, T *location) const noexcept(can_nothrow_initialize_elem<T>::value)
                {
                    target->template apply_to_elem<detail::allocator_hack::construct_from_elem_at<T, Alloc>>(index, alloc, location);
                }
                #endif
            };

            // The iterator class.
            // Homogeneous lists ignore `T` here.
            template <typename T>
            class elem_iter
            {
                friend BETTERLISTINIT_IDENTIFIER;
                const std::conditional_t<is_homogeneous, std::remove_reference_t<homogeneous_type> *, elem_ref<T>> *ptr = nullptr;

                template <typename Void = void, std::enable_if_t<detail::dependent_value<Void, !is_homogeneous>::value, detail::nullptr_t> = nullptr>
                constexpr auto &deref() const
                {
                    return *ptr;
                }
                template <typename Void = void, std::enable_if_t<detail::dependent_value<Void, is_homogeneous>::value, detail::nullptr_t> = nullptr>
                constexpr auto &&deref() const
                {
                    return static_cast<homogeneous_type>(**ptr);
                }

              public:
                // Can't use C++20 iterator category auto-detection here, since an rvalue reference `reference` makes it think it's an input iterator.
                // Yes, the detection logic is specified to not match the actual iterator requirements, this is LWG issue: https://cplusplus.github.io/LWG/issue3798
                using iterator_category = std::random_access_iterator_tag;
                using reference = std::conditional_t<is_homogeneous, homogeneous_type, const elem_ref<T> &>;
                using value_type = std::remove_cv_t<std::remove_reference_t<reference>>;
                using pointer = void;
                using difference_type = detail::ptrdiff_t;

                constexpr elem_iter() noexcept {}

                // `LegacyForwardIterator` requires us to return an actual reference here.
                constexpr reference operator*() const noexcept {return deref();}

                // No `operator->`. This causes C++20 `std::iterator_traits` to guess `pointer_type == void`, which sounds ok to me.

                // Don't want to rely on `<compare>`.
                friend constexpr bool operator==(elem_iter a, elem_iter b) noexcept
                {
                    return a.ptr == b.ptr;
                }
                friend constexpr bool operator!=(elem_iter a, elem_iter b) noexcept
                {
                    return !(a == b);
                }
                friend constexpr bool operator<(elem_iter a, elem_iter b) noexcept
                {
                    // Don't want to include `<functional>` for `std::less`, so need to cast to an integer to avoid UB.
                    return detail::uintptr_t(a.ptr) < detail::uintptr_t(b.ptr);
                }
                friend constexpr bool operator> (elem_iter a, elem_iter b) noexcept {return b < a;}
                friend constexpr bool operator<=(elem_iter a, elem_iter b) noexcept {return !(b < a);}
                friend constexpr bool operator>=(elem_iter a, elem_iter b) noexcept {return !(a < b);}

                constexpr elem_iter &operator++() noexcept
                {
                    ++ptr;
                    return *this;
                }
                constexpr elem_iter &operator--() noexcept
                {
                    --ptr;
                    return *this;
                }
                constexpr elem_iter operator++(int) noexcept
                {
                    elem_iter ret = *this;
                    ++*this;
                    return ret;
                }
                constexpr elem_iter operator--(int) noexcept
                {
                    elem_iter ret = *this;
                    --*this;
                    return ret;
                }
                constexpr friend elem_iter operator+(elem_iter it, detail::ptrdiff_t n) noexcept {it += n; return it;}
                constexpr friend elem_iter operator+(detail::ptrdiff_t n, elem_iter it) noexcept {it += n; return it;}
                constexpr friend elem_iter operator-(elem_iter it, detail::ptrdiff_t n) noexcept {it -= n; return it;}
                // There's no `number - iterator`.

                constexpr friend detail::ptrdiff_t operator-(elem_iter a, elem_iter b) noexcept {return a.ptr - b.ptr;}

                constexpr elem_iter &operator+=(detail::ptrdiff_t n) noexcept {ptr += n; return *this;}
                constexpr elem_iter &operator-=(detail::ptrdiff_t n) noexcept {ptr -= n; return *this;}

                constexpr reference operator[](detail::ptrdiff_t i) const noexcept
                {
                    return *(*this + i);
                }
            };

            // Could use `[[no_unique_address]]`, but it's our only member variable anyway.
            // Can't store `elem_ref`s here directly, because we can't use a templated `operator T` in our elements,
            // because it doesn't work correctly on MSVC (but not on GCC and Clang).
            tuple_t elems;

            // See the public `_helper`-less versions below.
            // Note that we have to use a specialization here. Directly inheriting from `integral_constant` isn't enough, because it's not SFINAE-friendly (with respect to the `element_type<T>::type`).
            template <typename Void, typename T, typename ...Q> struct can_initialize_range_helper : std::false_type {};
            template <typename T, typename ...Q> struct can_initialize_range_helper        <std::enable_if_t<custom::is_range<T>::value && detail::constructible_from_iters        <T, elem_iter<typename custom::element_type<T>::type>, BETTERLISTINIT_IDENTIFIER, Q...>::value && can_initialize_elem        <typename custom::element_type<T>::type>::value>, T, Q...> : std::true_type {};
            template <typename Void, typename T, typename ...Q> struct can_nothrow_initialize_range_helper : std::false_type {};
            template <typename T, typename ...Q> struct can_nothrow_initialize_range_helper<std::enable_if_t<custom::is_range<T>::value && detail::nothrow_constructible_from_iters<T, elem_iter<typename custom::element_type<T>::type>, BETTERLISTINIT_IDENTIFIER, Q...>::value && can_nothrow_initialize_elem<typename custom::element_type<T>::type>::value>, T, Q...> : std::true_type {};

          public:
            // Whether this list can be used to initialize a range type `T`, with extra constructor arguments `Q...`.
            template <typename T, typename ...Q> struct can_initialize_range         : can_initialize_range_helper        <void, T, Q...> {};
            template <typename T, typename ...Q> struct can_nothrow_initialize_range : can_nothrow_initialize_range_helper<void, T, Q...> {};
            // Whether this list can be used to initialize a non-range of type `T` (by forwarding arguments to the constructor), with extra constructor arguments `Q...`.
            // NOTE: We check `!is_range<T>` here to avoid the uniform init fiasco, at least for our lists.
            // NOTE: We need short-circuiting here, otherwise libstdc++ 10 in C++14 mode fails with a hard error in `nonrange_brace_constructible`.
            // NOTE: `sizeof...(Q) == 0` is an arbitrary restriction, hopefully forcing a more clear usage.
            template <typename T, typename ...Q> struct can_initialize_nonrange         : detail::all_of<detail::negate<custom::is_range<T>>, std::integral_constant<bool, sizeof...(Q) == 0>, detail::nonrange_brace_constructible        <T, BETTERLISTINIT_IDENTIFIER, P..., Q...>> {};
            template <typename T, typename ...Q> struct can_nothrow_initialize_nonrange : detail::all_of<detail::negate<custom::is_range<T>>, std::integral_constant<bool, sizeof...(Q) == 0>, detail::nothrow_nonrange_brace_constructible<T, BETTERLISTINIT_IDENTIFIER, P..., Q...>> {};

          private:
            template <typename T>
            struct convert_functor
            {
                const BETTERLISTINIT_IDENTIFIER *list = nullptr;

                // Convert to an empty range.
                template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...>::value && sizeof...(P) == 0, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr T operator()(Q &&... extra_args) const
                {
                    using elem_type = typename custom::element_type<T>::type;
                    return custom::construct_range<void, T, elem_iter<elem_type>, BETTERLISTINIT_IDENTIFIER, Q...>{}(elem_iter<elem_type>{}, elem_iter<elem_type>{}, static_cast<Q &&>(extra_args)...);
                }
                // Convert to a non-empty heterogeneous range.
                template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...>::value && sizeof...(P) != 0 && !is_homogeneous, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr T operator()(Q &&... extra_args) const
                {
                    using elem_type = typename custom::element_type<T>::type;

                    // Must store `elem_ref`s here, because `std::random_access_iterator` requires `operator[]` to return the same type as `operator*`,
                    // and `LegacyForwardIterator` requires `operator*` to return an actual reference. If we don't have those here, we don't have anything for the references to point to.
                    typename elem_ref<elem_type>::template array<sizeof...(P)> refs;
                    for (detail::size_t i = 0; i < sizeof...(P); i++)
                    {
                        refs.elems[i].target = &list->elems;
                        refs.elems[i].index = i;
                    }

                    elem_iter<elem_type> begin, end;
                    begin.ptr = refs.elems;
                    end.ptr = refs.elems + sizeof...(P);

                    return custom::construct_range<void, T, elem_iter<elem_type>, BETTERLISTINIT_IDENTIFIER, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
                }
                // Convert to a non-empty homogeneous range.
                template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...>::value && sizeof...(P) != 0 && is_homogeneous, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr T operator()(Q &&... extra_args) const
                {
                    using elem_type = typename custom::element_type<T>::type;

                    elem_iter<elem_type> begin, end;
                    begin.ptr = list->elems.values;
                    end.ptr = list->elems.values + sizeof...(P);

                    return custom::construct_range<void, T, elem_iter<elem_type>, BETTERLISTINIT_IDENTIFIER, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
                }
                // Convert to a non-range.
                template <typename ...Q, std::enable_if_t<can_initialize_nonrange<T, Q...>::value, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr T operator()(Q &&... extra_args) const
                {
                    detail::tuple<Q &&...> extra_tuple{&extra_args...};
                    using func_t = custom::construct_nonrange<void, T, BETTERLISTINIT_IDENTIFIER, P..., Q...>;
                    return extra_tuple.apply(detail::apply_functor<func_t, const tuple_t &>{func_t{}, list->elems});
                }
            };

            template <typename Void, typename T, typename ...Q> struct can_nothrow_initialize_helper : can_nothrow_initialize_nonrange<T, Q...> {};
            template <typename T, typename ...Q> struct can_nothrow_initialize_helper<std::enable_if_t<custom::is_range<T>::value>, T, Q...> : can_nothrow_initialize_range<T, Q...> {};

            template <typename Void, typename T, typename ...Q>
            struct allow_implicit_init_helper : custom::allow_implicit_nonrange_init<void, T, P..., Q...> {};
            template <typename T, typename ...Q>
            struct allow_implicit_init_helper<std::enable_if_t<custom::is_range<T>::value>, T, Q...> : custom::allow_implicit_range_init<void, T, Q.../*note, no P...*/> {};

          public:
            // Whether this list can be used to initialize a type `T`, with extra constructor arguments `Q...`.
            // Returns true if `can_initialize_range<T,Q...>` or `can_initialize_nonrange<T,Q...>` is true.
            template <typename T, typename ...Q> struct can_initialize : detail::any_of<can_initialize_range<T, Q...>, can_initialize_nonrange<T, Q...>> {};
            template <typename T, typename ...Q> struct can_nothrow_initialize : can_nothrow_initialize_helper<void, T, Q...> {};

            // Whether the conversion to `T` should be implicit, for extra constructor arguments `Q...`.
            template <typename T, typename ...Q>
            struct allow_implicit_init : allow_implicit_init_helper<void, T, Q...> {};

            // The constructor from a braced (or parenthesized) list.
            // No `[[nodiscard]]` because GCC 9 complains. Having it on the entire class should be enough.
            constexpr BETTERLISTINIT_IDENTIFIER(P &&... params) noexcept
                : elems{&params...}
            {}

            // Only copyable if `is_lvalue_only` is true.

            // The conversion functions below are `&&`-qualified as a reminder that your initializer elements can be dangling,
            // unless the list only contains lvalues.

            // Conversion operators.
            // Those work with both ranges (constructible from a pair of iterators) and non-ranges (constructible from braced lists).
            // Note that non-ranges can't use `(...)`, because `std::array` is a non-range and we want to support it too (including pre-C++20).
            // Note that the uniform init fiasco is impossible for our lists, since we allow braced init only if `detail::is_range<T>` is false.

            // Implicit, lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T>::value && allow_implicit_init<T>::value && is_lvalue_only, detail::nullptr_t> = nullptr>
            BETTERLISTINIT_NODISCARD constexpr operator T() const & noexcept(can_nothrow_initialize<T>::value)
            {
                return convert_functor<T>{this}();
            }
            // Explicit, lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T>::value && !allow_implicit_init<T>::value && is_lvalue_only, detail::nullptr_t> = nullptr>
            BETTERLISTINIT_NODISCARD constexpr explicit operator T() const & noexcept(can_nothrow_initialize<T>::value)
            {
                return convert_functor<T>{this}();
            }
            // Implicit, non-lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T>::value && allow_implicit_init<T>::value && !is_lvalue_only, detail::nullptr_t> = nullptr>
            BETTERLISTINIT_NODISCARD constexpr operator T() const && noexcept(can_nothrow_initialize<T>::value)
            {
                return convert_functor<T>{this}();
            }
            // Explicit, non-lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T>::value && !allow_implicit_init<T>::value && !is_lvalue_only, detail::nullptr_t> = nullptr>
            BETTERLISTINIT_NODISCARD constexpr explicit operator T() const && noexcept(can_nothrow_initialize<T>::value)
            {
                return convert_functor<T>{this}();
            }

          private:
            // This is used to convert to ranges with extra constructor arguments.
            template <typename ...Q>
            class conversion_helper
            {
                friend BETTERLISTINIT_IDENTIFIER;
                const BETTERLISTINIT_IDENTIFIER *list = nullptr;
                detail::tuple<Q &&...> extra_params;

                constexpr conversion_helper(const BETTERLISTINIT_IDENTIFIER *list, detail::tuple<Q &&...> extra_params)
                    : list(list), extra_params(extra_params)
                {}

              public:
                // Implicit, lvalue-only lists.
                template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...>::value && allow_implicit_init<T, Q...>::value && is_lvalue_only, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr operator T() const & noexcept(can_nothrow_initialize<T, Q...>::value)
                {
                    return extra_params.apply(convert_functor<T>{list});
                }
                // Explicit, lvalue-only lists.
                template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...>::value && !allow_implicit_init<T, Q...>::value && is_lvalue_only, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr explicit operator T() const & noexcept(can_nothrow_initialize<T, Q...>::value)
                {
                    return extra_params.apply(convert_functor<T>{list});
                }
                // Implicit, non-lvalue-only lists.
                template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...>::value && allow_implicit_init<T, Q...>::value && !is_lvalue_only, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr operator T() const && noexcept(can_nothrow_initialize<T, Q...>::value)
                {
                    return extra_params.apply(convert_functor<T>{list});
                }
                // Explicit, non-lvalue-only lists.
                template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...>::value && !allow_implicit_init<T, Q...>::value && !is_lvalue_only, detail::nullptr_t> = nullptr>
                BETTERLISTINIT_NODISCARD constexpr explicit operator T() const && noexcept(can_nothrow_initialize<T, Q...>::value)
                {
                    return extra_params.apply(convert_functor<T>{list});
                }
            };

          public:
            // Returns a helper object with conversion operators.
            // `extra_params...` are the extra parameters passed to the type's constructor, after the pair of iterators.
            template <typename ...Q>
            BETTERLISTINIT_NODISCARD constexpr conversion_helper<Q &&...> and_with(Q &&... extra_params) const && noexcept
            {
                return {this, {&extra_params...}};
            }

            // Begin/end iterators, for homogeneous lists only.

            // Lvalue-only.
            template <detail::deduce..., typename Void = void, std::enable_if_t<detail::dependent_value<Void, is_homogeneous && is_lvalue_only>::value, detail::nullptr_t> = nullptr>
            elem_iter<Void> begin() const & noexcept
            {
                elem_iter<Void> ret;
                ret.ptr = elems.values;
                return ret;
            }
            template <detail::deduce..., typename Void = void, std::enable_if_t<detail::dependent_value<Void, is_homogeneous && is_lvalue_only>::value, detail::nullptr_t> = nullptr>
            elem_iter<Void> end() const & noexcept
            {
                elem_iter<Void> ret;
                ret.ptr = elems.values + sizeof...(P);
                return ret;
            }
            // Non-lvalue-only.
            template <detail::deduce..., typename Void = void, std::enable_if_t<detail::dependent_value<Void, is_homogeneous && !is_lvalue_only>::value, detail::nullptr_t> = nullptr>
            elem_iter<Void> begin() const && noexcept
            {
                elem_iter<Void> ret;
                ret.ptr = elems.values;
                return ret;
            }
            template <detail::deduce..., typename Void = void, std::enable_if_t<detail::dependent_value<Void, is_homogeneous && !is_lvalue_only>::value, detail::nullptr_t> = nullptr>
            elem_iter<Void> end() const && noexcept
            {
                elem_iter<Void> ret;
                ret.ptr = elems.values + sizeof...(P);
                return ret;
            }
        };

        #if BETTERLISTINIT_ALLOW_BRACES
        template <typename ...P>
        BETTERLISTINIT_IDENTIFIER(P &&...) -> BETTERLISTINIT_IDENTIFIER<P...>;
        // Fix the `list(list)` deduction.
        template <typename ...P> BETTERLISTINIT_IDENTIFIER(      BETTERLISTINIT_IDENTIFIER<P...> &&) -> BETTERLISTINIT_IDENTIFIER<      BETTERLISTINIT_IDENTIFIER<P...>>;
        template <typename ...P> BETTERLISTINIT_IDENTIFIER(const BETTERLISTINIT_IDENTIFIER<P...> &&) -> BETTERLISTINIT_IDENTIFIER<const BETTERLISTINIT_IDENTIFIER<P...>>;
        template <typename ...P> BETTERLISTINIT_IDENTIFIER(      BETTERLISTINIT_IDENTIFIER<P...> & ) -> BETTERLISTINIT_IDENTIFIER<      BETTERLISTINIT_IDENTIFIER<P...> &>;
        template <typename ...P> BETTERLISTINIT_IDENTIFIER(const BETTERLISTINIT_IDENTIFIER<P...> & ) -> BETTERLISTINIT_IDENTIFIER<const BETTERLISTINIT_IDENTIFIER<P...> &>;
        #endif
    }

    #if BETTERLISTINIT_ALLOW_BRACES
    using type::BETTERLISTINIT_IDENTIFIER;
    #else
    // A helper function to construct the list class.
    template <typename ...P>
    BETTERLISTINIT_NODISCARD constexpr type::BETTERLISTINIT_IDENTIFIER<P...> BETTERLISTINIT_IDENTIFIER(P &&... params) noexcept
    {
        // Note, not doing `return T(...);`. There's difference in C++14, when there's no mandatory copy elision.
        return {static_cast<P &&>(params)...};
    }
    #endif
}

// The shorthand in the global namespace.
#if BETTERLISTINIT_SHORTHAND
using better_list_init::BETTERLISTINIT_IDENTIFIER;
#endif

// The stable ways to interact with the library, unaffected by `BETTERLISTINIT_IDENTIFIER`.
// Those are intended for dependent library implementers.
#define BETTERLISTINIT_INIT ::better_list_init::BETTERLISTINIT_IDENTIFIER
#define BETTERLISTINIT_TYPE ::better_list_init::type::BETTERLISTINIT_IDENTIFIER


// The allocator hack.
// See the macro definition for details.
#if BETTERLISTINIT_ALLOCATOR_HACK
namespace better_list_init
{
    namespace detail
    {
        namespace allocator_hack
        {
            // Check if we're affected by the bug.
            // This also returns true if we don't have the mandatory copy elision,
            // but running the hack pre-C++20 is only useful for testing purposes (see the comments on `BETTERLISTINIT_ALLOCATOR_HACK`).
            #if BETTERLISTINIT_ALLOCATOR_HACK > 1 || !HAVE_MANDATORY_COPY_ELISION
            struct enabled : std::true_type {};
            #else
            struct construct_at_checker
            {
                constexpr construct_at_checker(int) {}
                construct_at_checker(const construct_at_checker &) = delete;
                construct_at_checker &operator=(const construct_at_checker &) = delete;
            };

            struct construct_at_checker_init
            {
                constexpr operator construct_at_checker() {return construct_at_checker(42);}
            };

            template <typename T = construct_at_checker, typename = void>
            struct enabled_helper : std::true_type {};
            template <typename T>
            struct enabled_helper<T, decltype(void(std::construct_at((T *)nullptr, construct_at_checker_init{})))> : std::false_type {};
            struct enabled : enabled_helper<> {};
            #endif

            // The hacked allocator we use.
            template <typename Base> struct modified_allocator;
            // Whether `T` is a specialization of our hacked allocator.
            template <typename T> struct is_modified_allocator : std::false_type {};
            template <typename T> struct is_modified_allocator<modified_allocator<T>> : std::true_type {};

            // Whether `T` is an allocator class that we can replace.
            // `T` must be non-final, since we're going to inherit from it.
            // We could work around final allocators by making it a member and forwarding a bunch of calls to it, but that's too much work.
            // Also `T` can't be a specialization of our `modified_allocator<T>`.
            template <typename T, typename = void, typename = void>
            struct is_replaceable_allocator : std::false_type {};
            template <typename T>
            struct is_replaceable_allocator<T,
                decltype(
                    std::enable_if_t<
                        !std::is_final<T>::value &&
                        !is_modified_allocator<std::remove_cv_t<std::remove_reference_t<T>>>::value
                    >(),
                    void(declval<typename T::value_type>()),
                    void(declval<T &>().deallocate(declval<T &>().allocate(size_t{}), size_t{}))
                ),
                // Check this last, because `std::allocator_traits` are not SFINAE-friendly to non-allocators.
                // This is in a separate template parameter, because GCC doesn't abort early enough otherwise.
                std::enable_if_t<
                    std::is_same<typename std::allocator_traits<T>::pointer, typename std::allocator_traits<T>::value_type *>::value
                >
            > : std::true_type {};

            // If T satisfies `is_replaceable_allocator`, return our `modified_allocator` for it. Otherwise return `T` unchanged.
            // You're allowed to specialize `substitute_allocator_type`.
            // We separate the implementation into `default_substitute_allocator_type` to not mess with user specializations.
            template <typename T, typename = void>
            struct default_substitute_allocator_type {using type = T;};
            template <typename T>
            struct default_substitute_allocator_type<T, std::enable_if_t<is_replaceable_allocator<T>::value>> {using type = modified_allocator<T>;};
            template <typename T, typename = void>
            struct substitute_allocator_type : default_substitute_allocator_type<T> {};

            // Apply `substitute_allocator_type` to each template argument of `T`, recursively.
            // You're allowed to specialize `replace_allocator`.
            // We separate the implementation into `default_replace_allocator` to not mess with user specializations.
            // Note the SFINAE check that gracefully handles `T<...>` rejecting our allocator in a SFINAE-friendly manner.
            template <typename T, typename = void>
            struct default_replace_allocator {using type = T;};
            template <template <typename...> class T, typename ...P>
            struct default_replace_allocator<T<P...>, typename first_type<void, T<typename substitute_allocator_type<P>::type...>>::type>
            {
                using type = T<typename substitute_allocator_type<P>::type...>;
            };
            template <typename T, typename = void>
            struct replace_allocator : default_replace_allocator<T> {};

            // Whether `T` has an allocator template argument that can be replaced.
            // That is, whether `replace_allocator` changes anything in `T`.
            // You're allowed to specialize `has_replaceable_allocator`.
            // We separate the implementation into `default_has_replaceable_allocator` to not mess with user specializations.
            // While we don't expect a SFINAE failure from `replace_allocator` here, an extra SFINAE safeguard doesn't hurt.
            template <typename T, typename = void>
            struct default_has_replaceable_allocator : std::false_type {};
            template <typename T>
            struct default_has_replaceable_allocator<T, std::enable_if_t<!std::is_same<T, typename replace_allocator<T>::type>::value>> : std::true_type {};
            template <typename T, typename = void>
            struct has_replaceable_allocator : default_has_replaceable_allocator<T> {};

            // Whether `T` is a reference class. If it's used as a `.construct()` parameter, we wrap this call.
            template <typename T>
            struct is_elem_ref : std::is_base_of<elem_ref_base, std::remove_cv_t<std::remove_reference_t<T>>> {};

            // Whether `P...` contains a single type that satisfies `is_elem_ref`.
            template <typename Void, typename ...P>
            struct is_one_elem_ref_helper : std::false_type {};
            template <typename T>
            struct is_one_elem_ref_helper<std::enable_if_t<is_elem_ref<T>::value>, T> : std::true_type {};
            template <typename ...P>
            struct is_one_elem_ref : is_one_elem_ref_helper<void, P...> {};

            // Whether the allocator `T` defines `.construct(declval<P>()...)`.
            // If it doesn't do so, we can replace `allocator_traits<T>::construct()` with placement-new.
            // Note that `P[0]` is a pointer to the target location.
            // You're allowed to specialize `allocator_defines_construct_func`.
            // We separate the implementation into `default_allocator_defines_construct_func` to not mess with user specializations.
            template <typename Void, typename T, typename ...P>
            struct default_allocator_defines_construct_func : std::false_type {};
            #if !BETTERLISTINIT_ALLOCATOR_HACK_IGNORE_EXISTING_CONSTRUCT_FUNC
            template <typename T, typename ...P>
            struct default_allocator_defines_construct_func<decltype(void(declval<T>().construct(declval<P>()...))), T, P...> : std::true_type {};
            #endif
            template <typename Void, typename T, typename ...P>
            struct allocator_defines_construct_func : default_allocator_defines_construct_func<void, T, P...> {};

            // Whether initializing `T` with `U` uses `U`'s conversion operator to `T`.
            // You're allowed to specialize `is_conversion_operator_init`.
            // We separate the implementation into `default_is_conversion_operator_init` to not mess with user specializations.
            template <typename T, typename U, typename = void>
            struct default_is_conversion_operator_init : std::false_type {};
            template <typename T, typename U>
            struct default_is_conversion_operator_init<T, U, decltype(void(std::declval<U>().operator T()))> : std::true_type {};
            template <typename T, typename U, typename = void>
            struct is_conversion_operator_init : default_is_conversion_operator_init<T, U> {};

            template <typename Base>
            struct modified_allocator : Base
            {
                // Allocator traits use this. We can't use the inherited one, since its typedef doesn't point to our own class.
                // In C++20 `std::allocator` doesn't define this, but we have to support other allocators too, and we also want to work pre-C++20 for testing purposes.
                template <typename T>
                struct rebind
                {
                    using other = modified_allocator<typename std::allocator_traits<Base>::template rebind_alloc<T>>;
                };

                // Solely for convenience. The rest of typedefs are inherited.
                using value_type = typename std::allocator_traits<Base>::value_type;

                // Note that the functions below need to be defined in a specific order (defined before being called), otherwise Clang complains.
                // Probably because they're mentioned in each other's `noexcept` specifications.

                // Note that the pointer parameter type HAS TO be templated. See the allocator requirements. MSVC's `std::map` actively uses this (uses `alloc<_Tree_node>` to construct pairs).

                // Variant: `P...` is NOT our reference class, AND the allocator does NOT have a custom `.construct()`.
                template <typename T, typename ...P, std::enable_if_t<
                    !is_one_elem_ref<P...>::value &&
                    !allocator_defines_construct_func<Base, T *, P &&...>::value
                , nullptr_t> = nullptr>
                constexpr void construct(T *ptr, P &&... params)
                noexcept(std::is_nothrow_constructible<T, P &&...>::value)
                {
                    ::new((void *)ptr) T(static_cast<P &&>(params)...);
                }

                // Variant: `P...` is NOT our reference class, AND the allocator DOES have a custom `.construct()`.
                template <typename T, typename ...P, std::enable_if_t<
                    !is_one_elem_ref<P...>::value &&
                    allocator_defines_construct_func<Base, T *, P &&...>::value
                , nullptr_t> = nullptr>
                constexpr void construct(T *ptr, P &&... params)
                noexcept(std::allocator_traits<Base>::construct(declval<Base &>(), declval<T *>(), declval<P &&>()...))
                {
                    std::allocator_traits<Base>::construct(static_cast<Base &>(*this), ptr, static_cast<P &&>(params)...);
                }

                // Variant: `P...` IS our reference class.
                template <typename T, typename U, typename ...P, std::enable_if_t<
                    is_one_elem_ref<U, P...>::value
                , nullptr_t> = nullptr>
                constexpr void construct(T *ptr, U &&param, P &&...)
                noexcept(noexcept(declval<U &&>()._allocator_hack_construct_at(*this, declval<T *>())))
                {
                    static_cast<U &&>(param)._allocator_hack_construct_at(*this, ptr);
                }
            };
        }

        template <typename T, typename Iter, typename List, typename ...P>
        struct default_construct_range<
            std::enable_if_t<
                detail::allocator_hack::enabled::value && // If the compile-time detection finds the `std::construct_at()` bug, and
                (
                    !List::is_homogeneous || // either the list is heterogeneous, or
                    detail::allocator_hack::is_conversion_operator_init<typename custom::element_type<T>::type, typename List::homogeneous_type>::value // it's homogeneous, but the initialization uses `operator T`
                ) && // and...
                !std::is_move_constructible<typename custom::element_type<T>::type>::value && // if the type is not move-constructible, and
                detail::allocator_hack::has_replaceable_allocator<T>::value // if there's an allocator we can replace.
            >,
            T, Iter, List, P...
        >
        {
            using fixed_container = typename detail::allocator_hack::replace_allocator<T>::type;

            constexpr T operator()(Iter begin, Iter end, P &&... params) const
            noexcept(noexcept(custom::construct_range<void, fixed_container, Iter, List, P...>{}(detail::declval<Iter &&>(), detail::declval<Iter &&>(), detail::declval<P &&>()...)))
            {
                // Note that we intentionally `reinterpret_cast` (which requires `may_alias` and all that),
                // rather than memcpy-ing into the proper type. That's because the container might remember its own address.
                struct BETTERLISTINIT_ALLOCATOR_HACK_MAY_ALIAS alias_from {fixed_container value;};
                alias_from ret{custom::construct_range<void, fixed_container, Iter, List, P...>{}(static_cast<Iter &&>(begin), static_cast<Iter &&>(end), static_cast<P &&>(params)...)};
                struct BETTERLISTINIT_ALLOCATOR_HACK_MAY_ALIAS alias_to {T value;};
                static_assert(sizeof(alias_from) == sizeof(alias_to) && alignof(alias_from) == alignof(alias_to), "Internal error: Our custom allocator has a wrong size or alignment.");
                return reinterpret_cast<alias_to &&>(ret).value;
            }
        };
    }
}
#endif
