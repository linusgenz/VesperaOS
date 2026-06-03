// sys_spawn.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 21.09.25.
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

#include "../user_copy.h"
#include <exec/spawn.h>

namespace syscalls::internal {

    static constexpr usize MAX_ARGS    = 16;
    static constexpr usize MAX_ARG_LEN = 256;

    i64 sys_spawn(u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64, u64) {
        char path[MAX_ARG_LEN];
        if (!copy_str_from_user(path, reinterpret_cast<const char*>(arg0), sizeof(path)))
            return -EFAULT;

        char         argv_storage[MAX_ARGS][MAX_ARG_LEN];
        const char*  kernel_argv[MAX_ARGS + 1]{};

        const i64 argc = copy_argv_from_user(
            kernel_argv, argv_storage, MAX_ARGS,
            reinterpret_cast<const char**>(arg1)
        );
        if (argc < 0) return -EFAULT;

        char         envp_storage[MAX_ARGS][MAX_ARG_LEN];
        const char*  kernel_envp[MAX_ARGS + 1]{};

        const i64 envc = copy_argv_from_user(
            kernel_envp, envp_storage, MAX_ARGS,
            reinterpret_cast<const char**>(arg2)
        );
        if (envc < 0) return -EFAULT;

        spawn_config_t cfg{};
        const spawn_config_t* cfg_ptr = nullptr;
        if (arg3) {
            if (!copy_from_user(&cfg, reinterpret_cast<const void*>(arg3), sizeof(cfg)))
                return -EFAULT;
            cfg_ptr = &cfg;
        }

        return SYSCALL_TRY(kernel::exec::spawn(path, kernel_argv, kernel_envp, cfg_ptr));
    }

}  // namespace syscalls::internal
