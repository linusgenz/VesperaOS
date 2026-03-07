// concepts.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.03.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
#ifndef VESPERAOS_CONCEPTS_H
#define VESPERAOS_CONCEPTS_H

#include <klib/type_traits.h>

namespace klib
{

// ============================================================
//  Kern-Konzepte
// ============================================================

// same_as — bidirektional (A == B && B == A)
template <typename T, typename U>
concept same_as = klib::is_same_v<T, U> && klib::is_same_v<U, T>;

// derived_from
template <typename Derived, typename Base>
concept derived_from =
    klib::is_base_of_v<Base, Derived> &&
    klib::is_convertible_v<const volatile Derived*, const volatile Base*>;

// convertible_to
template <typename From, typename To>
concept convertible_to =
    klib::is_convertible_v<From, To> &&
    requires { static_cast<To>(klib::declval<From>()); };

// common_with — benötigt klib::common_type, das separat implementiert werden muss.
// Aktivieren sobald common_type im type_traits-Header vorhanden ist:
//
// template <typename T, typename U>
// concept common_with = requires
// {
//     typename klib::common_type_t<T, U>;
//     typename klib::common_type_t<U, T>;
// };


// ============================================================
//  Arithmetik & Typen
// ============================================================

template <typename T>
concept integral = klib::is_integral_v<T>;

template <typename T>
concept signed_integral = integral<T> && klib::is_signed_v<T>;

template <typename T>
concept unsigned_integral = integral<T> && klib::is_unsigned_v<T>;

template <typename T>
concept floating_point = klib::is_floating_point_v<T>;

template <typename T>
concept arithmetic = klib::is_arithmetic_v<T>;

template <typename T>
concept scalar = klib::is_scalar_v<T>;

template <typename T>
concept pointer = klib::is_pointer_v<T>;

template <typename T>
concept enum_type = klib::is_enum_v<T>;


// ============================================================
//  Objekt-Konzepte
// ============================================================

template <typename T>
concept destructible = klib::is_nothrow_destructible_v<T>;

template <typename T>
concept default_initializable =
    klib::is_default_constructible_v<T> &&
    requires { T{}; };

template <typename T>
concept move_constructible =
    klib::is_move_constructible_v<T> &&
    convertible_to<T, T>;

template <typename T>
concept copy_constructible =
    move_constructible<T> &&
    klib::is_copy_constructible_v<T>;


// ============================================================
//  Zuweisungs-Konzepte
// ============================================================

template <typename T>
concept movable =
    klib::is_object_v<T>          &&
    move_constructible<T>        &&
    klib::is_move_assignable_v<T>;

template <typename T>
concept copyable =
    movable<T>                   &&
    copy_constructible<T>        &&
    klib::is_copy_assignable_v<T>;

template <typename T>
concept semiregular = copyable<T> && default_initializable<T>;


// ============================================================
//  Vergleichs-Konzepte
// ============================================================

template <typename T>
concept equality_comparable = requires(const T& a, const T& b)
{
    { a == b } -> same_as<bool>;
    { a != b } -> same_as<bool>;
};

template <typename T, typename U>
concept equality_comparable_with =
    equality_comparable<T> &&
    equality_comparable<U> &&
    requires(const T& a, const U& b)
    {
        { a == b } -> same_as<bool>;
        { a != b } -> same_as<bool>;
        { b == a } -> same_as<bool>;
        { b != a } -> same_as<bool>;
    };

template <typename T>
concept totally_ordered =
    equality_comparable<T> &&
    requires(const T& a, const T& b)
    {
        { a <  b } -> same_as<bool>;
        { a >  b } -> same_as<bool>;
        { a <= b } -> same_as<bool>;
        { a >= b } -> same_as<bool>;
    };

template <typename T>
concept regular = semiregular<T> && equality_comparable<T>;


// ============================================================
//  Callable-Konzepte
// ============================================================

template <typename F, typename... Args>
concept invocable = requires(F&& f, Args&&... args)
{
    f(static_cast<Args&&>(args)...);
};

template <typename F, typename... Args>
concept predicate =
    invocable<F, Args...> &&
    requires(F&& f, Args&&... args)
    {
        { f(static_cast<Args&&>(args)...) } -> same_as<bool>;
    };

template <typename F, typename T, typename U = T>
concept binary_predicate = predicate<F, T, U>;

template <typename F, typename T>
concept unary_predicate = predicate<F, T>;


// ============================================================
//  Iterator-Konzepte (minimal, für Range-for / Vector-kompatibel)
// ============================================================

template <typename I>
concept incrementable = requires(I i)
{
    { ++i } -> same_as<I&>;
    { i++ } -> same_as<I>;
};

template <typename I>
concept input_iterator =
    requires(I i) { *i; }  &&
    incrementable<I>       &&
    equality_comparable<I>;

template <typename I>
concept forward_iterator = input_iterator<I>;

template <typename I>
concept bidirectional_iterator =
    forward_iterator<I> &&
    requires(I i)
    {
        { --i } -> same_as<I&>;
        { i-- } -> same_as<I>;
    };

template <typename I>
concept random_access_iterator =
    bidirectional_iterator<I> &&
    totally_ordered<I>        &&
    requires(I i, ptrdiff_t n)
    {
        { i + n  } -> same_as<I>;
        { i - n  } -> same_as<I>;
        { i - i  } -> same_as<ptrdiff_t>;
        { i[n]   };
    };


// ============================================================
//  Range-Konzept (passt zu deinem Vector)
// ============================================================

template <typename R>
concept range = requires(R& r)
{
    r.begin();
    r.end();
};

template <typename R>
concept sized_range =
    range<R> &&
    requires(const R& r) { { r.size() } -> same_as<usize>; };

template <typename R, typename T>
concept range_of =
    range<R> &&
    requires(R& r) { { *r.begin() } -> convertible_to<T>; };


} // namespace klib

#endif  // VESPERAOS_CONCEPTS_H
