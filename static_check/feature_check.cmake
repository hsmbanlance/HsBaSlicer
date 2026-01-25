# define check for C/C++ features
include(CheckCSourceCompiles)
include(CheckCXXSourceCompiles)

# Helper: check C feature: C11/C23 _Generic
function(check_c_feature_GENERIC result)
	check_c_source_compiles(
"#include <stdio.h>
#define CHK(x) _Generic((x), int:1, default:0)
int main(void){ return CHK(1); }
" ${result})
endfunction()

# C++ feature checks
function(check_cxx_feature_concepts result)
	check_cxx_source_compiles(
"template<typename T> concept C = true;
int main() { return 0; }
" ${result})
endfunction()

function(check_cxx_feature_ranges result)
	check_cxx_source_compiles(
"#include <ranges>
#include <vector>
static_assert(std::ranges::range<std::vector<int>>);
int main() { return 0; }
" ${result})
endfunction()

function(check_cxx_feature_source_location result)
	check_cxx_source_compiles(
"#include <source_location>
int main(){ auto loc = std::source_location::current(); (void)loc; }
" ${result})
endfunction()

# Non-type template parameter: class-type
function(check_cxx_feature_nttp_class result)
	check_cxx_source_compiles(
"struct S { int v; };
constexpr S s{1};
template <S> struct X {};
X<s> x;
int main(){ return 0; }
" ${result})
endfunction()

# Non-type template parameter: floating point
function(check_cxx_feature_nttp_fp result)
	check_cxx_source_compiles(
"constexpr double d = 3.1415;
template<double> struct Y {};
Y<d> y;
int main(){ return 0; }
" ${result})
endfunction()

# Template-template parameter matching (P0522R0 style)
function(check_cxx_feature_template_template_matching result)
	check_cxx_source_compiles(
"template<template<typename, int = 0> class TT> struct S {};
template<typename T, int N = 0> struct A {};
S<A> s;
int main(){ return 0; }
" ${result})
endfunction()

# Optional: C++ coroutines
function(check_cxx_feature_coroutines result)
	check_cxx_source_compiles(
"#include <coroutine>
struct A {
	struct promise_type {
		A get_return_object() { return {}; }
		std::suspend_never initial_suspend() { return {}; }
		std::suspend_never final_suspend() noexcept { return {}; }
		void return_void() {}
		void unhandled_exception() {}
	};
};
A f(){ co_return; }
int main(){ return 0; }
" ${result})
endfunction()

# Optional: explicit object parameter (explicit this)
function(check_cxx_feature_explicit_this result)
	check_cxx_source_compiles(
"struct S { void foo(this S& self, int) {} };
int main(){ S s; s.foo(1); return 0; }
" ${result})
endfunction()

# Optional: C23 constexpr-like support check (try compile with C2x-style constexpr usage)
function(check_c_feature_c23_constexpr result)
	check_c_source_compiles(
"#if __STDC_VERSION__ >= 202300L
/* try a simple consteval-like usage in C23 (implementation-defined) */
#define MAYBE_CONSTEXPR constexpr
#else
#define MAYBE_CONSTEXPR
#endif
MAYBE_CONSTEXPR int a = 42;
int main(void){ (void)a; return 1; }
" ${result})
endfunction()

