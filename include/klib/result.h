// Result.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 30.04.26.
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

#include <klib/error.h>
#include <vespera/types.h>

#include "../../kernel/utils/panic.h"

namespace klib {

    template <typename T>
    class [[nodiscard]] Result {
       public:
        static Result ok(const T& value) {
            Result r;
            r.ok_ = true;
            new (&r.storage_.value) T(value);
            return r;
        }

        static Result err(Error e) {
            Result r;
            r.ok_ = false;
            r.storage_.err = e;
            return r;
        }

        ~Result() {
            if (ok_) {
                storage_.value.~T();
            }
        }

        Result(const Result& other)
            : ok_(other.ok_) {
            if (ok_) {
                new (&storage_.value) T(other.storage_.value);
            } else {
                storage_.err = other.storage_.err;
            }
        }

        Result& operator=(const Result& other) {
            if (this == &other) {
                return *this;
            }
            if (ok_) {
                storage_.value.~T();
            }
            ok_ = other.ok_;
            if (ok_) {
                new (&storage_.value) T(other.storage_.value);
            } else {
                storage_.err = other.storage_.err;
            }
            return *this;
        }

        Result(Result&& other) noexcept
            : ok_(other.ok_) {
            if (ok_) {
                new (&storage_.value) T(static_cast<T&&>(other.storage_.value));
            } else {
                storage_.err = other.storage_.err;
            }
        }

        Result& operator=(Result&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            if (ok_) {
                storage_.value.~T();
            }
            ok_ = other.ok_;
            if (ok_) {
                new (&storage_.value) T(static_cast<T&&>(other.storage_.value));
            } else {
                storage_.err = other.storage_.err;
            }
            return *this;
        }

        [[nodiscard]] bool is_ok() const {
            return ok_;
        }
        [[nodiscard]] bool is_err() const {
            return !ok_;
        }

        [[nodiscard]] explicit operator bool() const {
            return ok_;
        }

        [[nodiscard]] T& value() {
            if (!ok_) [[unlikely]] {
                panic("Tried to access value() on error Result");
            }
            return storage_.value;
        }
        [[nodiscard]] const T& value() const {
            if (!ok_) {
                panic("Tried to access value() on error Result");
            }
            return storage_.value;
        }

        [[nodiscard]] Error err_code() const {
            return storage_.err;
        }

        [[nodiscard]] i32 to_errno() const {
            return ok_ ? 0 : static_cast<i32>(storage_.err);
        }

        [[nodiscard]] Error to_error() const {
            return ok_ ? Error::SUCCESS : storage_.err;
        }

       private:
        Result() = default;

        bool ok_ = false;

        union Storage {
            T value;
            Error err;

            Storage() {
            }
            ~Storage() {
            }
        } storage_;
    };

    template <>
    class [[nodiscard]] Result<void> {
       public:
        static Result ok() {
            return Result(true);
        }

        static Result err(Error e) {
            return Result(e);
        }

        [[nodiscard]] bool is_ok() const {
            return ok_;
        }
        [[nodiscard]] bool is_err() const {
            return !ok_;
        }

        [[nodiscard]] explicit operator bool() const {
            return ok_;
        }

        [[nodiscard]] Error err_code() const {
            return err_;
        }

        [[nodiscard]] Error to_error() const {
            return ok_ ? Error::SUCCESS : err_;
        }

        [[nodiscard]] i32 to_errno() const {
            return ok_ ? 0 : static_cast<i32>(err_);
        }

       private:
        explicit Result(bool ok)
            : ok_(ok)
            , err_(Error::SUCCESS) {
        }

        explicit Result(Error e)
            : ok_(false)
            , err_(e) {
        }

        bool ok_;
        Error err_;
    };

}  // namespace klib
using klib::Result;
#endif  // VESPERAOS_KLIB_RESULT_H
