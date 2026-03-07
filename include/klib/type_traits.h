// type_traits.h
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
#ifndef VESPERAOS_TYPE_TRAITS_H
#define VESPERAOS_TYPE_TRAITS_H

#include <stddef.h>

// ReSharper disable CppInconsistentNaming
namespace klib {

    template <typename T, T v>
    struct integral_constant {
        static constexpr T value = v;
        using value_type = T;
        using type = integral_constant;
        constexpr operator value_type() const noexcept {
            return value;
        }
        constexpr value_type operator()() const noexcept {
            return value;
        }
    };

    using true_type = integral_constant<bool, true>;
    using false_type = integral_constant<bool, false>;

    // bool_constant
    template <bool B>
    using bool_constant = integral_constant<bool, B>;

    template <bool B, typename T = void>
    struct enable_if {};

    template <typename T>
    struct enable_if<true, T> {
        using type = T;
    };

    template <bool B, typename T = void>
    using enable_if_t = typename enable_if<B, T>::type;

    // conditional
    template <bool B, typename T, typename F>
    struct conditional {
        using type = T;
    };

    template <typename T, typename F>
    struct conditional<false, T, F> {
        using type = F;
    };

    template <bool B, typename T, typename F>
    using conditional_t = typename conditional<B, T, F>::type;

    // type_identity
    template <typename T>
    struct type_identity {
        using type = T;
    };

    template <typename T>
    using type_identity_t = typename type_identity<T>::type;

    // void_t
    template <typename...>
    using void_t = void;

    template <typename T>
    struct remove_const {
        using type = T;
    };
    template <typename T>
    struct remove_const<const T> {
        using type = T;
    };

    template <typename T>
    struct remove_volatile {
        using type = T;
    };
    template <typename T>
    struct remove_volatile<volatile T> {
        using type = T;
    };

    template <typename T>
    struct remove_cv {
        using type = typename remove_const<typename remove_volatile<T>::type>::type;
    };

    template <typename T>
    using remove_const_t = typename remove_const<T>::type;
    template <typename T>
    using remove_volatile_t = typename remove_volatile<T>::type;
    template <typename T>
    using remove_cv_t = typename remove_cv<T>::type;

    // add_const / add_volatile / add_cv
    template <typename T>
    struct add_const {
        using type = const T;
    };
    template <typename T>
    struct add_volatile {
        using type = volatile T;
    };
    template <typename T>
    struct add_cv {
        using type = const volatile T;
    };

    template <typename T>
    using add_const_t = typename add_const<T>::type;
    template <typename T>
    using add_volatile_t = typename add_volatile<T>::type;
    template <typename T>
    using add_cv_t = typename add_cv<T>::type;

    template <typename T>
    struct remove_reference {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T&> {
        using type = T;
    };
    template <typename T>
    struct remove_reference<T&&> {
        using type = T;
    };

    template <typename T>
    using remove_reference_t = typename remove_reference<T>::type;

    // add_lvalue_reference / add_rvalue_reference
    namespace detail {
        template <typename T, typename = void>
        struct add_lvalue_reference_impl {
            using type = T;
        };
        template <typename T>
        struct add_lvalue_reference_impl<T, void_t<T&>> {
            using type = T&;
        };

        template <typename T, typename = void>
        struct add_rvalue_reference_impl {
            using type = T;
        };
        template <typename T>
        struct add_rvalue_reference_impl<T, void_t<T&&>> {
            using type = T&&;
        };
    }  // namespace detail

    template <typename T>
    struct add_lvalue_reference : detail::add_lvalue_reference_impl<T> {};
    template <typename T>
    struct add_rvalue_reference : detail::add_rvalue_reference_impl<T> {};

    template <typename T>
    using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;
    template <typename T>
    using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

    // remove_cvref
    template <typename T>
    struct remove_cvref {
        using type = remove_cv_t<remove_reference_t<T>>;
    };
    template <typename T>
    using remove_cvref_t = typename remove_cvref<T>::type;

    // declval (nur in unevaluierten Kontexten!)
    template <typename T>
    add_rvalue_reference_t<T> declval() noexcept;

    template <typename T>
    struct remove_pointer {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T*> {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T* const> {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T* volatile> {
        using type = T;
    };
    template <typename T>
    struct remove_pointer<T* const volatile> {
        using type = T;
    };

    template <typename T>
    using remove_pointer_t = typename remove_pointer<T>::type;

    namespace detail {
        template <typename T, typename = void>
        struct add_pointer_impl {
            using type = T;
        };
        template <typename T>
        struct add_pointer_impl<T, void_t<remove_reference_t<T>*>> {
            using type = remove_reference_t<T>*;
        };
    }  // namespace detail
    template <typename T>
    struct add_pointer : detail::add_pointer_impl<T> {};
    template <typename T>
    using add_pointer_t = typename add_pointer<T>::type;

    template <typename T>
    struct remove_extent {
        using type = T;
    };
    template <typename T>
    struct remove_extent<T[]> {
        using type = T;
    };
    template <typename T, size_t N>
    struct remove_extent<T[N]> {
        using type = T;
    };
    template <typename T>
    using remove_extent_t = typename remove_extent<T>::type;

    template <typename T>
    struct remove_all_extents {
        using type = T;
    };
    template <typename T>
    struct remove_all_extents<T[]> {
        using type = typename remove_all_extents<T>::type;
    };
    template <typename T, size_t N>
    struct remove_all_extents<T[N]> {
        using type = typename remove_all_extents<T>::type;
    };
    template <typename T>
    using remove_all_extents_t = typename remove_all_extents<T>::type;

    template <typename T, typename U>
    struct is_same : false_type {};
    template <typename T>
    struct is_same<T, T> : true_type {};

    template <typename T, typename U>
    inline constexpr bool is_same_v = is_same<T, U>::value;

    template <typename T>
    struct is_void : is_same<void, remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_void_v = is_void<T>::value;

    template <typename T>
    struct is_null_pointer : is_same<decltype(nullptr), remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;

    // is_integral
    namespace detail {
        template <typename T>
        struct is_integral_base : false_type {};
        template <>
        struct is_integral_base<bool> : true_type {};
        template <>
        struct is_integral_base<char> : true_type {};
        template <>
        struct is_integral_base<signed char> : true_type {};
        template <>
        struct is_integral_base<unsigned char> : true_type {};
        template <>
        struct is_integral_base<short> : true_type {};
        template <>
        struct is_integral_base<unsigned short> : true_type {};
        template <>
        struct is_integral_base<int> : true_type {};
        template <>
        struct is_integral_base<unsigned int> : true_type {};
        template <>
        struct is_integral_base<long> : true_type {};
        template <>
        struct is_integral_base<unsigned long> : true_type {};
        template <>
        struct is_integral_base<long long> : true_type {};
        template <>
        struct is_integral_base<unsigned long long> : true_type {};
#if defined(__SIZEOF_INT128__)
        template <>
        struct is_integral_base<__int128> : true_type {};
        template <>
        struct is_integral_base<unsigned __int128> : true_type {};
#endif
        template <>
        struct is_integral_base<char16_t> : true_type {};
        template <>
        struct is_integral_base<char32_t> : true_type {};
        template <>
        struct is_integral_base<wchar_t> : true_type {};
#if defined(__cpp_char8_t)
        template <>
        struct is_integral_base<char8_t> : true_type {};
#endif
    }  // namespace detail

    template <typename T>
    struct is_integral : detail::is_integral_base<remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_integral_v = is_integral<T>::value;

    // is_floating_point
    namespace detail {
        template <typename T>
        struct is_floating_point_base : false_type {};
        template <>
        struct is_floating_point_base<float> : true_type {};
        template <>
        struct is_floating_point_base<double> : true_type {};
        template <>
        struct is_floating_point_base<long double> : true_type {};
    }  // namespace detail
    template <typename T>
    struct is_floating_point : detail::is_floating_point_base<remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

    // is_array
    template <typename T>
    struct is_array : false_type {};
    template <typename T>
    struct is_array<T[]> : true_type {};
    template <typename T, size_t N>
    struct is_array<T[N]> : true_type {};
    template <typename T>
    inline constexpr bool is_array_v = is_array<T>::value;

    // is_pointer
    namespace detail {
        template <typename T>
        struct is_pointer_base : false_type {};
        template <typename T>
        struct is_pointer_base<T*> : true_type {};
    }  // namespace detail
    template <typename T>
    struct is_pointer : detail::is_pointer_base<remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_pointer_v = is_pointer<T>::value;

    // is_lvalue_reference / is_rvalue_reference / is_reference
    template <typename T>
    struct is_lvalue_reference : false_type {};
    template <typename T>
    struct is_lvalue_reference<T&> : true_type {};
    template <typename T>
    inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

    template <typename T>
    struct is_rvalue_reference : false_type {};
    template <typename T>
    struct is_rvalue_reference<T&&> : true_type {};
    template <typename T>
    inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

    template <typename T>
    struct is_reference : bool_constant<is_lvalue_reference_v<T> || is_rvalue_reference_v<T>> {};
    template <typename T>
    inline constexpr bool is_reference_v = is_reference<T>::value;

    // is_function — via Spezialisierungen für alle Funktionssignaturen
    template <typename T>
    struct is_function : false_type {};
    // Reguläre Funktionen
    template <typename R, typename... Args>
    struct is_function<R(Args...)> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) volatile> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const volatile> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) &> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) volatile&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const volatile&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) &&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const&&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) volatile&&> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args...) const volatile&&> : true_type {};
    // Variadic
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...)> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...) const> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...) volatile> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...) const volatile> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...) &> : true_type {};
    template <typename R, typename... Args>
    struct is_function<R(Args..., ...) &&> : true_type {};

    template <typename T>
    inline constexpr bool is_function_v = is_function<T>::value;

    // is_member_pointer
    namespace detail {
        template <typename T>
        struct is_member_pointer_base : false_type {};
        template <typename T, typename U>
        struct is_member_pointer_base<T U::*> : true_type {};
    }  // namespace detail
    template <typename T>
    struct is_member_pointer : detail::is_member_pointer_base<remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_member_pointer_v = is_member_pointer<T>::value;

    // is_member_function_pointer
    namespace detail {
        template <typename T>
        struct is_mfp_base : false_type {};
        template <typename T, typename U>
        struct is_mfp_base<T U::*> : is_function<T> {};
    }  // namespace detail
    template <typename T>
    struct is_member_function_pointer : detail::is_mfp_base<remove_cv_t<T>> {};
    template <typename T>
    inline constexpr bool is_member_function_pointer_v = is_member_function_pointer<T>::value;

    // is_member_object_pointer
    template <typename T>
    struct is_member_object_pointer : bool_constant<is_member_pointer_v<T> && !is_member_function_pointer_v<T>> {};
    template <typename T>
    inline constexpr bool is_member_object_pointer_v = is_member_object_pointer<T>::value;

    // is_enum / is_union / is_class — benötigen Compiler-Builtins
    template <typename T>
    struct is_enum : bool_constant<__is_enum(T)> {};
    template <typename T>
    struct is_union : bool_constant<__is_union(T)> {};
    template <typename T>
    struct is_class : bool_constant<__is_class(T)> {};

    template <typename T>
    inline constexpr bool is_enum_v = is_enum<T>::value;
    template <typename T>
    inline constexpr bool is_union_v = is_union<T>::value;
    template <typename T>
    inline constexpr bool is_class_v = is_class<T>::value;

    template <typename T>
    struct is_arithmetic : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};
    template <typename T>
    inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

    template <typename T>
    struct is_fundamental : bool_constant<is_arithmetic_v<T> || is_void_v<T> || is_null_pointer_v<T>> {};
    template <typename T>
    inline constexpr bool is_fundamental_v = is_fundamental<T>::value;

    template <typename T>
    struct is_scalar
        : bool_constant<
              is_arithmetic_v<T> || is_enum_v<T> || is_pointer_v<T> || is_member_pointer_v<T> || is_null_pointer_v<T>> {
    };
    template <typename T>
    inline constexpr bool is_scalar_v = is_scalar<T>::value;

    template <typename T>
    struct is_object : bool_constant<!is_function_v<T> && !is_reference_v<T> && !is_void_v<T>> {};
    template <typename T>
    inline constexpr bool is_object_v = is_object<T>::value;

    template <typename T>
    struct is_compound : bool_constant<!is_fundamental_v<T>> {};
    template <typename T>
    inline constexpr bool is_compound_v = is_compound<T>::value;

    template <typename T>
    struct is_const : false_type {};
    template <typename T>
    struct is_const<const T> : true_type {};
    template <typename T>
    inline constexpr bool is_const_v = is_const<T>::value;

    template <typename T>
    struct is_volatile : false_type {};
    template <typename T>
    struct is_volatile<volatile T> : true_type {};
    template <typename T>
    inline constexpr bool is_volatile_v = is_volatile<T>::value;

    // is_signed / is_unsigned
    namespace detail {
        template <typename T, bool = is_arithmetic_v<T>>
        struct is_signed_impl : bool_constant<T(-1) < T(0)> {};
        template <typename T>
        struct is_signed_impl<T, false> : false_type {};

        template <typename T, bool = is_arithmetic_v<T>>
        struct is_unsigned_impl : bool_constant<T(0) < T(-1)> {};
        template <typename T>
        struct is_unsigned_impl<T, false> : false_type {};
    }  // namespace detail
    template <typename T>
    struct is_signed : detail::is_signed_impl<T> {};
    template <typename T>
    struct is_unsigned : detail::is_unsigned_impl<T> {};
    template <typename T>
    inline constexpr bool is_signed_v = is_signed<T>::value;
    template <typename T>
    inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

    // is_base_of — Compiler-Builtin
    template <typename Base, typename Derived>
    struct is_base_of : bool_constant<__is_base_of(Base, Derived)> {};
    template <typename Base, typename Derived>
    inline constexpr bool is_base_of_v = is_base_of<Base, Derived>::value;

    // is_convertible
    namespace detail {
        template <typename From, typename To>
        struct is_convertible_impl {
           private:
            static void test(To);
            template <typename F>
            static true_type check(decltype(test(declval<F>()))*);
            template <typename>
            static false_type check(...);

           public:
            using type = decltype(check<From>(nullptr));
        };
    }  // namespace detail
    template <typename From, typename To>
    struct is_convertible : detail::is_convertible_impl<From, To>::type {};
    template <typename From, typename To>
    inline constexpr bool is_convertible_v = is_convertible<From, To>::value;

    template <typename T>
    struct is_trivially_destructible : bool_constant<__is_trivially_destructible(T)> {};
    template <typename T>
    inline constexpr bool is_trivially_destructible_v = is_trivially_destructible<T>::value;

    template <typename T>
    struct is_trivially_copyable : bool_constant<__is_trivially_copyable(T)> {};
    template <typename T>
    inline constexpr bool is_trivially_copyable_v = is_trivially_copyable<T>::value;

    template <typename T>
    struct is_trivial : bool_constant<__is_trivial(T)> {};
    template <typename T>
    inline constexpr bool is_trivial_v = is_trivial<T>::value;

    template <typename T, typename... Args>
    struct is_trivially_constructible : bool_constant<__is_trivially_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_trivially_constructible_v = is_trivially_constructible<T, Args...>::value;

    template <typename T>
    struct is_trivially_default_constructible : is_trivially_constructible<T> {};
    template <typename T>
    inline constexpr bool is_trivially_default_constructible_v = is_trivially_default_constructible<T>::value;

    template <typename T>
    struct is_trivially_copy_constructible : is_trivially_constructible<T, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_trivially_copy_constructible_v = is_trivially_copy_constructible<T>::value;

    template <typename T>
    struct is_trivially_move_constructible : is_trivially_constructible<T, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_trivially_move_constructible_v = is_trivially_move_constructible<T>::value;

    template <typename T, typename U>
    struct is_trivially_assignable : bool_constant<__is_trivially_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_trivially_assignable_v = is_trivially_assignable<T, U>::value;

    template <typename T>
    struct is_trivially_copy_assignable
        : is_trivially_assignable<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_trivially_copy_assignable_v = is_trivially_copy_assignable<T>::value;

    template <typename T>
    struct is_trivially_move_assignable
        : is_trivially_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_trivially_move_assignable_v = is_trivially_move_assignable<T>::value;

    template <typename T, typename... Args>
    struct is_constructible : bool_constant<__is_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_constructible_v = is_constructible<T, Args...>::value;

    template <typename T>
    struct is_default_constructible : is_constructible<T> {};
    template <typename T>
    inline constexpr bool is_default_constructible_v = is_default_constructible<T>::value;

    template <typename T>
    struct is_copy_constructible : is_constructible<T, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_copy_constructible_v = is_copy_constructible<T>::value;

    template <typename T>
    struct is_move_constructible : is_constructible<T, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_move_constructible_v = is_move_constructible<T>::value;

    template <typename T, typename U>
    struct is_assignable : bool_constant<__is_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_assignable_v = is_assignable<T, U>::value;

    template <typename T>
    struct is_copy_assignable : is_assignable<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_copy_assignable_v = is_copy_assignable<T>::value;

    template <typename T>
    struct is_move_assignable : is_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_move_assignable_v = is_move_assignable<T>::value;

    template <typename T>
    struct is_destructible {
       private:
        template <typename U>
        static auto test(int) -> decltype(declval<U&>().~U(), true_type{});
        template <typename>
        static false_type test(...);

       public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    template <>
    struct is_destructible<void> : false_type {};
    template <typename T>
    struct is_destructible<T[]> : false_type {};
    template <typename T>
    inline constexpr bool is_destructible_v = is_destructible<T>::value;

    // nothrow-Varianten
    template <typename T, typename... Args>
    struct is_nothrow_constructible : bool_constant<__is_nothrow_constructible(T, Args...)> {};
    template <typename T, typename... Args>
    inline constexpr bool is_nothrow_constructible_v = is_nothrow_constructible<T, Args...>::value;

    template <typename T>
    struct is_nothrow_default_constructible : is_nothrow_constructible<T> {};
    template <typename T>
    inline constexpr bool is_nothrow_default_constructible_v = is_nothrow_default_constructible<T>::value;

    template <typename T>
    struct is_nothrow_copy_constructible : is_nothrow_constructible<T, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_nothrow_copy_constructible_v = is_nothrow_copy_constructible<T>::value;

    template <typename T>
    struct is_nothrow_move_constructible : is_nothrow_constructible<T, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_nothrow_move_constructible_v = is_nothrow_move_constructible<T>::value;

    template <typename T, typename U>
    struct is_nothrow_assignable : bool_constant<__is_nothrow_assignable(T, U)> {};
    template <typename T, typename U>
    inline constexpr bool is_nothrow_assignable_v = is_nothrow_assignable<T, U>::value;

    template <typename T>
    struct is_nothrow_copy_assignable
        : is_nothrow_assignable<add_lvalue_reference_t<T>, add_lvalue_reference_t<const T>> {};
    template <typename T>
    inline constexpr bool is_nothrow_copy_assignable_v = is_nothrow_copy_assignable<T>::value;

    template <typename T>
    struct is_nothrow_move_assignable : is_nothrow_assignable<add_lvalue_reference_t<T>, add_rvalue_reference_t<T>> {};
    template <typename T>
    inline constexpr bool is_nothrow_move_assignable_v = is_nothrow_move_assignable<T>::value;

    template <typename T>
    struct is_nothrow_destructible {
       private:
        template <typename U>
        static bool_constant<noexcept(declval<U&>().~U())> test(int);
        template <typename>
        static false_type test(...);

       public:
        static constexpr bool value = decltype(test<T>(0))::value;
    };
    template <typename T>
    inline constexpr bool is_nothrow_destructible_v = is_nothrow_destructible<T>::value;

    template <typename T>
    [[nodiscard]] constexpr remove_reference_t<T>&& move(T&& t) noexcept {
        return static_cast<remove_reference_t<T>&&>(t);
    }

    template <typename T>
    [[nodiscard]] constexpr T&& forward(remove_reference_t<T>& t) noexcept {
        return static_cast<T&&>(t);
    }

    template <typename T>
    [[nodiscard]] constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
        static_assert(!is_lvalue_reference_v<T>, "forward: cannot forward rvalue as lvalue");
        return static_cast<T&&>(t);
    }

    template <typename T>
    constexpr void swap(T& a, T& b) noexcept(is_nothrow_move_constructible_v<T> && is_nothrow_move_assignable_v<T>) {
        T tmp = move(a);
        a = move(b);
        b = move(tmp);
    }

    template <typename...>
    struct conjunction : true_type {};
    template <typename B>
    struct conjunction<B> : B {};
    template <typename B, typename... Bs>
    struct conjunction<B, Bs...> : conditional_t<bool(B::value), conjunction<Bs...>, B> {};
    template <typename... Bs>
    inline constexpr bool conjunction_v = conjunction<Bs...>::value;

    template <typename...>
    struct disjunction : false_type {};
    template <typename B>
    struct disjunction<B> : B {};
    template <typename B, typename... Bs>
    struct disjunction<B, Bs...> : conditional_t<bool(B::value), B, disjunction<Bs...>> {};
    template <typename... Bs>
    inline constexpr bool disjunction_v = disjunction<Bs...>::value;

    template <typename B>
    struct negation : bool_constant<!bool(B::value)> {};
    template <typename B>
    inline constexpr bool negation_v = negation<B>::value;

    template <size_t Len, size_t Align = alignof(max_align_t)>
    struct aligned_storage {
        struct type {
            alignas(Align) unsigned char data[Len];
        };
    };
    template <size_t Len, size_t Align = alignof(max_align_t)>
    using aligned_storage_t = typename aligned_storage<Len, Align>::type;

    namespace detail {
        template <typename T>
        struct make_signed_impl {
            using type = T;
        };
        template <>
        struct make_signed_impl<unsigned char> {
            using type = signed char;
        };
        template <>
        struct make_signed_impl<unsigned short> {
            using type = short;
        };
        template <>
        struct make_signed_impl<unsigned int> {
            using type = int;
        };
        template <>
        struct make_signed_impl<unsigned long> {
            using type = long;
        };
        template <>
        struct make_signed_impl<unsigned long long> {
            using type = long long;
        };
        template <>
        struct make_signed_impl<char> {
            using type = signed char;
        };

        template <typename T>
        struct make_unsigned_impl {
            using type = T;
        };
        template <>
        struct make_unsigned_impl<signed char> {
            using type = unsigned char;
        };
        template <>
        struct make_unsigned_impl<short> {
            using type = unsigned short;
        };
        template <>
        struct make_unsigned_impl<int> {
            using type = unsigned int;
        };
        template <>
        struct make_unsigned_impl<long> {
            using type = unsigned long;
        };
        template <>
        struct make_unsigned_impl<long long> {
            using type = unsigned long long;
        };
        template <>
        struct make_unsigned_impl<char> {
            using type = unsigned char;
        };
    }  // namespace detail

    template <typename T>
    struct make_signed {
        using type = typename detail::make_signed_impl<remove_cv_t<T>>::type;
    };
    template <typename T>
    using make_signed_t = typename make_signed<T>::type;

    template <typename T>
    struct make_unsigned {
        using type = typename detail::make_unsigned_impl<remove_cv_t<T>>::type;
    };
    template <typename T>
    using make_unsigned_t = typename make_unsigned<T>::type;

    namespace detail {
        template <typename F, typename... Args>
        struct invoke_result_impl {
            using type = decltype(declval<F>()(declval<Args>()...));
        };
    }  // namespace detail
    template <typename F, typename... Args>
    struct invoke_result : detail::invoke_result_impl<F, Args...> {};
    template <typename F, typename... Args>
    using invoke_result_t = typename invoke_result<F, Args...>::type;

}  // namespace klib

#endif  // VESPERAOS_TYPE_TRAITS_H
