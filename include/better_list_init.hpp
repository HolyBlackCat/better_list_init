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
#define BETTERLISTINIT_VERSION 20100
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
        // The default value for `custom::is_range`, see below.
        template <typename T, typename = void>
        struct default_is_range : std::false_type {};
        template <typename T>
        struct default_is_range<T, decltype(void(declval<typename custom::element_type<T>::type>()))> : std::integral_constant<bool, !std::is_aggregate_v<T>> {};
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

// Whether the `reinterpret_cast` to `void *` is constexpr. This is a C++26 feature.
#if __cpp_constexpr >= 202306
#define BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST 1
#else
#define BETTERLISTINIT_HAVE_CONSTEXPR_VOID_PTR_CAST 0
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

namespace better_list_init
{
    namespace detail
    {
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

        // Returns true if all types in `P...` are the same.
        template <typename ...P>
        constexpr bool all_types_same = true;
        template <typename T, typename ...P>
        constexpr bool all_types_same<T, P...> = (std::is_same_v<T, P> && ...);

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

        #if !BETTERLISTINIT_USE_VOID_PTR_ARRAY_AS_TUPLE
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
        #else // if BETTERLISTINIT_USE_VOID_PTR_ARRAY_AS_TUPLE
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
        struct tuple_impl_selector<std::enable_if_t<all_types_same<P...> && sizeof...(P) != 0>, P...>
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
        using enable_if_valid_conversion_target = std::enable_if_t<!std::is_const_v<T>, int>;
    }

    template <typename ...P>
    class [[nodiscard]] init
        : detail::maybe_copyable<(std::is_lvalue_reference_v<P> && ...)>
    {
      public:
        // Whether this list can be used to initialize a range of `T`s.
        template <typename T> static constexpr bool can_initialize_elem         = (detail::constructible        <T, P>::value && ...);
        template <typename T> static constexpr bool can_nothrow_initialize_elem = (detail::nothrow_constructible<T, P>::value && ...);

        // Whether all types in `P...` are the same (and there is at least one type). Then we can simplify some logic.
        static constexpr bool is_homogeneous = detail::all_types_same<P...> && sizeof...(P) > 0;
        // If all types in `P...` are the same (and there's at least one), returns that type. Otherwise returns an empty struct.
        using homogeneous_type = typename std::conditional_t<is_homogeneous, detail::first_type<P &&...>, std::enable_if<true, detail::empty>>::type;

        // Whether all our references are lvalue references. Such lists can be copied.
        static constexpr bool is_lvalue_only = (std::is_lvalue_reference_v<P> && ...);

      private:
        using tuple_t = detail::tuple<P &&...>;

        // Heterogeneous lists use this as the element type for the iterators.
        template <typename T>
        class elem_ref : detail::elem_ref_base
        {
            friend init;
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
            constexpr operator T() const noexcept(can_nothrow_initialize_elem<T>)
            {
                return target->template apply_to_elem<detail::construct_from_elem<T>>(index);
            }
        };

        // The iterator class.
        // Homogeneous lists ignore `T` here.
        template <typename T>
        class elem_iter
        {
            friend init;
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
        // This is specialized out-of-line to make older GCC versions happy.
        template <typename Void, typename T, typename ...Q> static constexpr bool can_initialize_range_helper         = false;
        // This is specialized out-of-line to make older GCC versions happy.
        template <typename Void, typename T, typename ...Q> static constexpr bool can_nothrow_initialize_range_helper = false;

      public:
        // Whether this list can be used to initialize a range type `T`, with extra constructor arguments `Q...`.
        template <typename T, typename ...Q> static constexpr bool can_initialize_range         = can_initialize_range_helper        <void, T, Q...>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize_range = can_nothrow_initialize_range_helper<void, T, Q...>;
        // Whether this list can be used to initialize a non-range of type `T` (by forwarding arguments to the constructor), with extra constructor arguments `Q...`.
        // NOTE: We check `!is_range<T>` here to avoid the uniform init fiasco, at least for our lists.
        // NOTE: `sizeof...(Q) == 0` is an arbitrary restriction, hopefully forcing a more clear usage.
        // NOTE: This needs short-circuiting to make MSVC happy. I believe older GCC versions need this too.
        template <typename T, typename ...Q> static constexpr bool can_initialize_nonrange         = std::conjunction_v<std::bool_constant<!custom::is_range<T>::value && sizeof...(Q) == 0>, detail::nonrange_brace_constructible        <T, init, P..., Q...>>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize_nonrange = std::conjunction_v<std::bool_constant<!custom::is_range<T>::value && sizeof...(Q) == 0>, detail::nothrow_nonrange_brace_constructible<T, init, P..., Q...>>;

      private:
        template <typename T>
        struct convert_functor
        {
            const init *list = nullptr;

            // Convert to an empty range.
            template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...> && sizeof...(P) == 0, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr T operator()(Q &&... extra_args) const
            {
                using elem_type = typename custom::element_type<T>::type;
                return custom::construct_range<void, T, elem_iter<elem_type>, init, Q...>{}(elem_iter<elem_type>{}, elem_iter<elem_type>{}, static_cast<Q &&>(extra_args)...);
            }
            // Convert to a non-empty heterogeneous range.
            template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...> && sizeof...(P) != 0 && !is_homogeneous, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr T operator()(Q &&... extra_args) const
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

                return custom::construct_range<void, T, elem_iter<elem_type>, init, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
            }
            // Convert to a non-empty homogeneous range.
            template <typename ...Q, std::enable_if_t<can_initialize_range<T, Q...> && sizeof...(P) != 0 && is_homogeneous, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr T operator()(Q &&... extra_args) const
            {
                using elem_type = typename custom::element_type<T>::type;

                elem_iter<elem_type> begin, end;
                begin.ptr = list->elems.values;
                end.ptr = list->elems.values + sizeof...(P);

                return custom::construct_range<void, T, elem_iter<elem_type>, init, Q...>{}(begin, end, static_cast<Q &&>(extra_args)...);
            }
            // Convert to a non-range.
            template <typename ...Q, std::enable_if_t<can_initialize_nonrange<T, Q...>, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr T operator()(Q &&... extra_args) const
            {
                detail::tuple<Q &&...> extra_tuple{&extra_args...};
                using func_t = custom::construct_nonrange<void, T, init, P..., Q...>;
                return extra_tuple.apply(detail::apply_functor<func_t, const tuple_t &>{func_t{}, list->elems});
            }
        };

        template <typename Void, typename T, typename ...Q> static constexpr bool can_nothrow_initialize_helper = can_nothrow_initialize_nonrange<T, Q...>;
        // This is specialized out-of-line to make older GCC versions happy.
        template <typename Void, typename T, typename ...Q> static constexpr bool allow_implicit_init_helper = custom::allow_implicit_nonrange_init<void, T, P..., Q...>::value;

      public:
        // Whether this list can be used to initialize a type `T`, with extra constructor arguments `Q...`.
        // Returns true if `can_initialize_range<T,Q...>` or `can_initialize_nonrange<T,Q...>` is true.
        template <typename T, typename ...Q> static constexpr bool can_initialize = can_initialize_range<T, Q...> || can_initialize_nonrange<T, Q...>;
        template <typename T, typename ...Q> static constexpr bool can_nothrow_initialize = can_nothrow_initialize_helper<void, T, Q...>;

        // Whether the conversion to `T` should be implicit, for extra constructor arguments `Q...`.
        template <typename T, typename ...Q>
        static constexpr bool allow_implicit_init = allow_implicit_init_helper<void, T, Q...>;

        // The constructor from a braced (or parenthesized) list.
        // No `[[nodiscard]]` because GCC 9 complains. Having it on the entire class should be enough.
        constexpr init(P &&... params) noexcept
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
        template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T> && allow_implicit_init<T> && is_lvalue_only, detail::nullptr_t> = nullptr>
        [[nodiscard]] constexpr operator T() const & noexcept(can_nothrow_initialize<T>)
        {
            return convert_functor<T>{this}();
        }
        // Explicit, lvalue-only lists.
        template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T> && !allow_implicit_init<T> && is_lvalue_only, detail::nullptr_t> = nullptr>
        [[nodiscard]] constexpr explicit operator T() const & noexcept(can_nothrow_initialize<T>)
        {
            return convert_functor<T>{this}();
        }
        // Implicit, non-lvalue-only lists.
        template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T> && allow_implicit_init<T> && !is_lvalue_only, detail::nullptr_t> = nullptr>
        [[nodiscard]] constexpr operator T() const && noexcept(can_nothrow_initialize<T>)
        {
            return convert_functor<T>{this}();
        }
        // Explicit, non-lvalue-only lists.
        template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T> && !allow_implicit_init<T> && !is_lvalue_only, detail::nullptr_t> = nullptr>
        [[nodiscard]] constexpr explicit operator T() const && noexcept(can_nothrow_initialize<T>)
        {
            return convert_functor<T>{this}();
        }

      private:
        // This is used to convert to ranges with extra constructor arguments.
        template <typename ...Q>
        class conversion_helper
        {
            friend init;
            const init *list = nullptr;
            detail::tuple<Q &&...> extra_params;

            constexpr conversion_helper(const init *list, detail::tuple<Q &&...> extra_params)
                : list(list), extra_params(extra_params)
            {}

          public:
            // Implicit, lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...> && allow_implicit_init<T, Q...> && is_lvalue_only, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr operator T() const & noexcept(can_nothrow_initialize<T, Q...>)
            {
                return extra_params.apply(convert_functor<T>{list});
            }
            // Explicit, lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...> && !allow_implicit_init<T, Q...> && is_lvalue_only, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr explicit operator T() const & noexcept(can_nothrow_initialize<T, Q...>)
            {
                return extra_params.apply(convert_functor<T>{list});
            }
            // Implicit, non-lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...> && allow_implicit_init<T, Q...> && !is_lvalue_only, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr operator T() const && noexcept(can_nothrow_initialize<T, Q...>)
            {
                return extra_params.apply(convert_functor<T>{list});
            }
            // Explicit, non-lvalue-only lists.
            template <typename T, detail::enable_if_valid_conversion_target<T> = 0, std::enable_if_t<can_initialize<T, Q...> && !allow_implicit_init<T, Q...> && !is_lvalue_only, detail::nullptr_t> = nullptr>
            [[nodiscard]] constexpr explicit operator T() const && noexcept(can_nothrow_initialize<T, Q...>)
            {
                return extra_params.apply(convert_functor<T>{list});
            }
        };

      public:
        // Returns a helper object with conversion operators.
        // `extra_params...` are the extra parameters passed to the type's constructor, after the pair of iterators.
        template <typename ...Q>
        [[nodiscard]] constexpr conversion_helper<Q &&...> and_with(Q &&... extra_params) const && noexcept
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

    template <typename ...P> template <typename T, typename ...Q> constexpr bool init<P...>::can_initialize_range_helper        <std::enable_if_t<custom::is_range<T>::value && detail::constructible_from_iters        <T, typename init<P...>::template elem_iter<typename custom::element_type<T>::type>, init<P...>, Q...>::value && init<P...>::template can_initialize_elem        <typename custom::element_type<T>::type>>, T, Q...> = true;
    template <typename ...P> template <typename T, typename ...Q> constexpr bool init<P...>::can_nothrow_initialize_range_helper<std::enable_if_t<custom::is_range<T>::value && detail::nothrow_constructible_from_iters<T, typename init<P...>::template elem_iter<typename custom::element_type<T>::type>, init<P...>, Q...>::value && init<P...>::template can_nothrow_initialize_elem<typename custom::element_type<T>::type>>, T, Q...> = true;
    template <typename ...P> template <typename T, typename ...Q> constexpr bool init<P...>::can_nothrow_initialize_helper<std::enable_if_t<custom::is_range<T>::value>, T, Q...> = can_nothrow_initialize_range<T, Q...>;
    template <typename ...P> template <typename T, typename ...Q> constexpr bool init<P...>::allow_implicit_init_helper<std::enable_if_t<custom::is_range<T>::value>, T, Q...> = custom::allow_implicit_range_init<void, T, Q.../*note, no P...*/>::value;

    template <typename ...P>
    init(P &&...) -> init<P...>;
}

// The shorthand in the global namespace.
#if BETTERLISTINIT_SHORTHAND
using better_list_init::init;
#endif
