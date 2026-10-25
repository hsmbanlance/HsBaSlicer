/** @file contracts.hpp
 * @brief Feature-macro-gated access to the C++26 contract specifiers `pre` and
 * `post` (P2900 / function contract specifiers). These are language keywords that
 * attach to the END of a function declarator - after the parameter list, cv/ref
 * qualifiers, trailing-noexcept and requires-clause, and before the body or
 * semicolon - NOT runtime helper functions and NOT the in-body `contract_assert`.
 * Availability is detected solely through the standard feature-test macro
 * __cpp_contracts (value 202502L, surfaced via <version>), which the compiler
 * predefines once the contract keywords are implemented. On toolchains without
 * support the HSBA_PRE / HSBA_POST macros expand to nothing so annotated
 * declarations stay well-formed. A small set of reusable predicate functions is
 * provided for use inside the contract conditions.
 * @author HsBa
 */

#ifndef HSBA_SLICER_CONTRACTS_HPP
#define HSBA_SLICER_CONTRACTS_HPP
#pragma once

#include <cstddef>
#include <limits>
#include <version>

// C++26 contracts are a language feature: there is no header that provides the
// keywords. Conformant compilers predefine __cpp_contracts (visible through
// <version>) once `contract_assert`, `pre` and `post` are available, so the
// standard feature-test macro is the single source of truth.
#ifdef __cpp_contracts
// Native path: forward to the `pre` / `post` function contract specifiers.
// __VA_ARGS__ lets call sites embed commas (e.g. a result-binding `post(r: ...)`)
// and multi-argument predicates without breaking the macro.
#define HSBA_PRE(...) pre(__VA_ARGS__)
#define HSBA_POST(...) post(__VA_ARGS__)
#else
// Fallback: contracts are not enforced on this toolchain, but the annotated
// declaration must remain valid C++, so the specifiers expand to nothing.
#define HSBA_PRE(...)
#define HSBA_POST(...)
#endif

namespace HsBa::Slicer::Contracts
{

/** @brief Reports whether a raw pointer is non-null.
 * @tparam T Pointed-to type.
 * @param p Pointer to test.
 */
template <typename T>
constexpr bool not_null(T* p) noexcept
{
    return p != nullptr;
}

/** @brief Reports whether a smart-pointer-like object owns a resource.
 * @tparam P Any type exposing a `.get()` member.
 * @param p Owner to test.
 */
template <typename P>
requires requires(const P& x)
{
    {x.get()};
}
constexpr bool not_null(const P& p) noexcept
{
    return p.get() != nullptr;
}

/** @brief Reports whether @p value lies inside the inclusive range [lo, hi].
 * Ordered via operator< so it works for any strict-weak-orderable type.
 */
template <typename T>
constexpr bool in_range(const T& value, const T& lo, const T& hi) noexcept
{
    return !(value < lo) && !(hi < value);
}

/** @brief Reports whether a container-like object holds at least one element.
 * @tparam C Any type exposing `.empty()`.
 */
template <typename C>
requires requires(const C& x)
{
    {x.empty()};
}
constexpr bool not_empty(const C& c) noexcept
{
    return !c.empty();
}

/** @brief Reports whether an index is within [0, size). */
constexpr bool valid_index(std::size_t index, std::size_t size) noexcept
{
    return index < size;
}

/** @brief Reports whether a signed value is strictly greater than zero. */
template <typename T>
constexpr bool is_positive(T v) noexcept
{
    return v > T{};
}

/** @brief Reports whether a signed value is greater than or equal to zero. */
template <typename T>
constexpr bool is_non_negative(T v) noexcept
{
    return !(v < T{});
}

/** @brief Reports whether a floating-point value is finite (not NaN and not infinite).
 * Implemented without <cmath> so it stays constexpr across toolchains.
 */
constexpr bool is_finite(double x) noexcept
{
    constexpr double inf = std::numeric_limits<double>::infinity();
    return x == x && x != inf && x != -inf;
}

}  // namespace HsBa::Slicer::Contracts

#endif  // HSBA_SLICER_CONTRACTS_HPP
