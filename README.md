# ~ better_list_init ~

**List-initialize containers with proper move semantics,<br/>
including containers of non-copyable elements, which otherwise don't support `std::initializer_list` constructors.**

[![tests badge](https://github.com/HolyBlackCat/better_list_init/actions/workflows/tests.yml/badge.svg?branch=master)](https://github.com/HolyBlackCat/better_list_init/actions?query=branch%3Amaster)<br/>
<kbd>[Try on gcc.godbolt.org](https://godbolt.org/z/sYnjEsYv7)</kbd>

<details><summary>Table of contents</summary>
<p>

* [The problem](#the-problem)
* [The solution](#the-solution)
  * [More examples](#more-examples)
* [How do I use this library?](#how-do-i-use-this-library)
* [How does it work?](#how-does-it-work)
* [A detailed explanation](#a-detailed-explanation)
  * [The nature of `init`](#the-nature-of-init)
  * [Range vs non-range initialization](#range-vs-non-range-initialization)
  * [More on range initialization](#more-on-range-initialization)
  * [`.begin()`/`.end()`](#beginend)

</p>
</details>

<!-- To regenerate the table of contents, run:
grep -E '^##+ ' README.md | sed -E -e 's/^## /* /g' -e 's/^### /  * /g' -e 's/^#### /    * /g' | gawk '{$0 = gensub(/( *\* )(.*)/,"\\1[\\2]","g") "(#" gensub(/[^-_a-z0-9]/,"","g",gensub(/ /,"-","g",tolower(gensub(/ *\* /,"",1,$0)))) ")"; print}'
-->

## The problem

`std::initializer_list` stores `const` elements, so the elements can't be moved from it. This causes unnecessary copying, and compilation errors for non-copyable types.

For example, this doesn't work, since `unique_ptr` is non-copyable:

```cpp
std::vector<std::unique_ptr<int>> vec = {nullptr, std::make_unique<int>(42)};
```

Neither does this, since `std::atomic` is not even movable:
```cpp
std::vector<std::atomic_int> vec = {1, 2, 3};
```

## The solution

This library provides a workaround. Just include the header and add `init` before the braces:

```cpp
#include <better_list_init.hpp>

std::vector<std::unique_ptr<int>> vec = init{nullptr, std::make_unique<int>(42)};
//                                      ^~~~
std::vector<std::atomic_int> vec = init{1, 2, 3};
//                                 ^~~~
```
This compiles and performs the initialization as you would expect.

In the first example, one could also use `.reserve()`+`.emplace_back()`, so `init{...}` just provides a prettier syntax.

In the second example, `.emplace_back()` wouldn't compile at all, because the type is non-movable.<br/>

### More examples

* **Initializing a map:** (or any nested container)

  ```cpp
  std::map<std::atomic_int, std::atomic_int> map = init{init{1,2}, init{3,4}};
  ```

  We support nested initializer lists, but the nested lists must use `init{...}` too, instead of just `{...}`.

* **Passing a custom allocator:** (or any extra constructor arguments)
  ```cpp
  std::vector<std::atomic_int, MyAllocator> vec = init{1, 2, 3}.and_with(MyAllocator(...));
  ```

* **Using `init{...}` as a list of references:**

  This requires all list elements to have exactly the same type.

  * With standard algorithms:

    ```cpp
    int x = 3, y = 2, z = 1;
    std::ranges::sort(init{x, y, z});
    std::cout << x << y << z << '\n'; // 123
    ```
    Or, pre-C++20:
    ```cpp
    auto list = init{x, y, z};
    std::sort(list.begin(), list.end());
    ```

  * With a `for` loop:
    ```cpp
    int x = 3, y = 2, z = 1;
    for (int &elem : init{x, y, z})
        elem++;
    ```
    This is equivalent to `for (int *elem : {&x, &y, &z}) (*elem)++;`, but with less annoying syntax.


## How do I use this library?

The library is header-only, with minimal standard library dependencies.

Just clone the repository, add the `include` directory to the header search path, and include `<better_list_init.hpp>`.

To improve the build times, avoid using `init{...}` when it doesn't bring any benefit, i.e. with types that don't benefit from being moved (and are copyable), such as scalars, especially large arrays thereof.

We currently test on Clang 15+, GCC 12+, and the latest MSVC, though earlier compiler versions might work as well. At least C++17 is required.

If you don't like `init` in the global namespace, you can spell it as `better_list_init::init`, and disable the short spelling by compiling with `-DBETTERLISTINIT_SHORTHAND=0` (or by creating a file called `better_list_init_config.hpp` with `#define BETTERLISTINIT_SHORTHAND 0` in it, in a directory where `#include "..."` can find it).

Study `namespace custom` and the macros defined in `better_list_init.hpp` for more customization capabilities.

## How does it work?

The container is constructed from a pair of iterators.

Those are custom iterators that deference to the provided initializers. The iterators are random-access, so containers should be initialized without reallocations.

A longer explanation is provided below.

## A detailed explanation

### The nature of `init`

`init<P...>` is a class template. `P...` are deduced as the types of the elements passed to `init{...}`, in a forward reference manner (deducing non-references for rvalue elements, and lvalue references for lvalue elements).

`init` stores pointers to the elements. If at least one element is an rvalue (i.e. can become dangling), all operations become `&&`-qualified.

`init` has a templated conversion `operator`, which can be used to construct containers.

### Range vs non-range initialization

First, we determine if the target type looks like a container or not, i.e. if we should be constructing it from a pair of iterators. I call those container-like types "ranges" here.

Non-ranges are initialized directly with the list elements, instead of the iterators. Ranges are never initialized directly with the elements, so we avoid the direct-list-initialization fiasco.

> By the direct-list-initialization fiasco I mean the sad fact that even though `std::vector<int> x{2};` has a single element `2`, `std::vector<std::string> x{2};` instead has two empty string elements (because you can't construct a string from `2`, the element-wise initialization can't be performed and falls back to this).
>
> Normally you work around this by inserting `=` before the `{`, which bans the latter scenario, as the offending constructor is `explicit`.
>
> Our `init{...}` never performs the latter.

**A type is a "range" if has a `::value_type` typedef, and is not an aggregate.** We reject aggregates because `std::array` is one, and we don't want to initialize it with a pair of iterators. We also want to know this typedef for other reasons, see below.

**Ranges are initialized using `T(begin, end)`. Non-ranges are initialized using `T{elements...}`.** You can pass extra arguments to a range constructor by feeding them to `.and_with(...)`. Non-ranges don't accept `.and_with(...)` at all (except with no arguments, to simplify generic code).

The conversion `operator` from `init{...}` to a type can sometimes be **`explicit`**, this happens when the underlying constructor we call is `explicit`. I.e. for ranges, we check the constructor that accepts the two iterators (followed by the arguments passed to `.and_with()`, if any).

### More on range initialization

Ranges are constructed from a pair of iterators, followed by the extra arguments passed to `.and_with()`, if any. The iterators are custom random-access iterators, so the container knows the target size immediately.

What the iterators deference to depends on whether the list is homogeneous.

I call an `init{...}` list **"homogeneous"** if all its elements have the same type, and there's at least one element. Otherwise it is **"heterogeneous"**. E.g. `init{1,2}` is homogeneous, but `init{1,2.0}` is not, `init{}` is not, and `int x;` `init{1,x}` is not (because the elements have different values categories: rvalue vs lvalue).

I call the sole element type of a homogeneous list its **"homogeneous type"**, it's always a reference (either an lvalue or rvalue one).

Homogeneous list iterators dereference to its homogeneous type. Heterogeneous list iterators deference to a (const reference to a) helper objects that has an `operator T`, where `T` is the `::value_type` of the container. (The initial idea was to template the `operator T`, but I ran into some problems with MSVC.)

### `.begin()`/`.end()`

Homogeneous lists expose `.begin()` and `.end()` as member functions. The iterators are random-access and dereference to the homogeneous type. Those are the same iterators that are used when constructing ranges.

Note that like all other member functions, `.begin()` and `.end()` become `&&`-qualified if the list contains at least one rvalue.
