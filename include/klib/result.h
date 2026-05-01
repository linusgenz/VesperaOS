// result.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 01.05.26.
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
#ifndef VESPERAOS_KLIB_RESULT_H
#define VESPERAOS_KLIB_RESULT_H
#include "../../kernel/utils/panic.h"
#include "vespera/types.h"
#include "vespera_errno.h"

namespace klib {

    enum class Error : int {
        Ok = 0,

        Perm = EPERM,                // EPERM
        NoEnt = ENOENT,              // ENOENT
        Srch = ESRCH,                // ESRCH
        Intr = EINTR,                // EINTR
        Io = EIO,                    // EIO
        NxIo = ENXIO,                // ENXIO
        TooBig = E2BIG,              // E2BIG
        NoExec = ENOEXEC,            // ENOEXEC
        BadH = EBADH,                // EBADH
        Child = ECHILD,              // ECHILD
        Again = EAGAIN,              // EAGAIN  (= EWOULDBLOCK)
        NoMem = ENOMEM,              // ENOMEM
        Acces = EACCES,              // EACCES
        Fault = EFAULT,              // EFAULT
        Busy = EBUSY,                // EBUSY
        Exist = EEXIST,              // EEXIST
        XDev = EXDEV,                // EXDEV
        NoDev = ENODEV,              // ENODEV
        NotDir = ENOTDIR,            // ENOTDIR
        IsDir = EISDIR,              // EISDIR
        Inval = EINVAL,              // EINVAL
        NFile = ENFILE,              // ENFILE
        MFile = EMFILE,              // EMFILE
        NotTty = ENOTTY,             // ENOTTY
        TxtBsy = ETXTBSY,            // ETXTBSY
        FBig = EFBIG,                // EFBIG
        NoSpc = ENOSPC,              // ENOSPC
        SPipe = ESPIPE,              // ESPIPE
        RoFs = EROFS,                // EROFS
        MLink = EMLINK,              // EMLINK
        Pipe = EPIPE,                // EPIPE
        Dom = EDOM,                  // EDOM
        Range = ERANGE,              // ERANGE
        NameTooLong = ENAMETOOLONG,  // ENAMETOOLONG
        NoLck = ENOLCK,              // ENOLCK
        NoSys = ENOSYS,              // ENOSYS
        NotEmpty = ENOTEMPTY,        // ENOTEMPTY
        Loop = ELOOP,                // ELOOP
        NoMsg = ENOMSG,              // ENOMSG
        Overflow = EOVERFLOW,        // EOVERFLOW
        IlSeq = EILSEQ,              // EILSEQ

        Unknown = EUNKNOWN,          // EUNKNOWN
        Unsupported = EUNSUPPORTED,  // EUNSUPPORTED
        Deadlock = EDEADLOCK,        // EDEADLOCK
    };

    template <typename T>
    struct Result {
       private:
        T value_;
        Error error_;

        constexpr Result(T v, Error e)
            : value_(v)
            , error_(e) {
        }

       public:
        constexpr Result(Error e)
            : value_{}
            , error_(e) {
        }

        [[nodiscard]] bool is_ok() const {
            return error_ == Error::Ok;
        }
        [[nodiscard]] bool is_err() const {
            return error_ != Error::Ok;
        }
        explicit operator bool() const {
            return is_ok();
        }

        [[nodiscard]] Error error() const {
            if (is_ok()) {
                panic("Result::error() called on ok state");
            }
            return error_;
        }

        [[nodiscard]] i32 to_errno() const {
            return -static_cast<i32>(error_);
        }

        [[nodiscard]] T& unwrap() {
            if (is_err()) {
                panic("Result::unwrap() called on error state");
            }
            return value_;
        }
        [[nodiscard]] const T& unwrap() const {
            if (is_err()) {
                panic("Result::unwrap() called on error state");
            }
            return value_;
        }

        [[nodiscard]] T& operator*() {
            return unwrap();
        }
        [[nodiscard]] const T& operator*() const {
            return unwrap();
        }
        [[nodiscard]] T* operator->() {
            return &unwrap();
        }

        [[nodiscard]] T value_or(const T& fallback) const {
            return is_ok() ? value_ : fallback;
        }

        [[nodiscard]] static constexpr Result ok(T v) {
            return {v, Error::Ok};
        }
        [[nodiscard]] static constexpr Result err(Error e) {
            return {{}, e};
        }
    };

    template <>
    struct Result<void> {
       private:
        Error error_;

       public:
        constexpr Result(Error e)
            : error_(e) {
        }

        [[nodiscard]] bool is_ok() const {
            return error_ == Error::Ok;
        }
        [[nodiscard]] bool is_err() const {
            return error_ != Error::Ok;
        }
        explicit operator bool() const {
            return is_ok();
        }

        [[nodiscard]] Error error() const {
            if (is_ok()) {
                panic("Result::error() called on ok state");
            }
            return error_;
        }

        [[nodiscard]] i32 to_errno() const {
            return -static_cast<i32>(error_);
        }

        [[nodiscard]] static constexpr Result ok() {
            return Result{Error::Ok};
        }
        [[nodiscard]] static constexpr Result err(Error e) {
            return Result{e};
        }
    };

    using VoidResult = Result<void>;
}  // namespace klib

#define TRY(expr)                             \
    ({                                        \
        auto _r = (expr);                     \
        if (_r.is_err()) return {_r.error()}; \
        _r.unwrap();                          \
    })

#define TRY_VOID(expr)                                       \
    do {                                                     \
        auto _r = (expr);                                    \
        if (_r.is_err()) return VoidResult::err(_r.error()); \
    } while (0)

#define SYSCALL_TRY(expr)                      \
    ({                                         \
        auto _r = (expr);                      \
        if (_r.is_err()) return _r.to_errno(); \
        _r.unwrap();                           \
    })

#define SYSCALL_TRY_VOID(expr)                 \
    do {                                       \
        auto _r = (expr);                      \
        if (_r.is_err()) return _r.to_errno(); \
    } while (0)

using Error = klib::Error;
template <typename T>
using Result = klib::Result<T>;
using VoidResult = klib::VoidResult;

#endif  // VESPERAOS_KLIB_RESULT_H
