// MSVC STL Compatibility Shims for Native Port
//
// Iterator fix: patched libc++ __wrap_iter.h in native/include/ adds
// implicit conversion from __wrap_iter<T*> to T*.
//
// This file provides additional compat shims for C++ features that
// differ between MSVC's old STL and modern libc++.

#pragma once

#if defined(HX_NATIVE) && defined(__cplusplus)

// std::random_shuffle was removed in C++17. libc++ enforces this.
// The DC3 codebase uses it in several places. Provide a compat shim.
#include <algorithm>
#include <cstdlib>

// Shim for std::random_shuffle, removed in C++17.
// Only needed when the standard library actually removed it (libc++, older libstdc++).
// GCC 15's libstdc++ re-added it, so skip to avoid redefinition.
#if defined(_LIBCPP_VERSION) || (defined(__GLIBCXX__) && __GLIBCXX__ < 20250101)
namespace std {
template <class RandomIt>
void random_shuffle(RandomIt first, RandomIt last) {
    for (auto i = last - first - 1; i > 0; --i) {
        auto j = std::rand() % (i + 1);
        std::swap(first[i], first[j]);
    }
}
} // namespace std
#endif

// Shim for std::mem_fun, removed in C++17. Same situation as random_shuffle.
#include <functional>
#if defined(_LIBCPP_VERSION) || (defined(__GLIBCXX__) && __GLIBCXX__ < 20250101)
namespace std {
template <class Ret, class T>
auto mem_fun(Ret (T::*f)()) { return std::mem_fn(f); }
template <class Ret, class T>
auto mem_fun(Ret (T::*f)() const) { return std::mem_fn(f); }
template <class Ret, class T, class Arg>
auto mem_fun(Ret (T::*f)(Arg)) { return std::mem_fn(f); }
} // namespace std
#endif

#endif // HX_NATIVE
