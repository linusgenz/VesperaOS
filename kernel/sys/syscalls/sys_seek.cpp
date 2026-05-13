// sys_seek.cpp
//
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
//
// Created by Linus Genz on 04.10.25.
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

#include <uapi/vespera/fflags.h>
#include <uapi/vespera/handles.h>

#include "../handle_resolution.h"
#include <filesystem/vfs_handle.h>

namespace syscalls::internal {
    i64 sys_seek(u64 arg0, u64 arg1, u64 arg2, u64, u64, u64) {
        const HandleId hid = arg0;
        const i64 offset = static_cast<i64>(arg1);
        const int whence = static_cast<int>(arg2);

        const auto rh = SYSCALL_TRY(resolve_handle(hid));

        switch (rh.type()) {
            case HANDLE_TYPE_DIRECTORY:
            case HANDLE_TYPE_TTY:
                return -ESPIPE;

            case HANDLE_TYPE_DEVICE:
            case HANDLE_TYPE_FILE: {
                const auto* vh = rh.resource_as<VfsHandle>();
                if (!vh || !vh->node) return -EBADH;

                i64 new_pos = 0;

                switch (whence) {
                    case SEEK_SET:
                        new_pos = offset;
                        break;
                    case SEEK_CUR:
                        new_pos = vh->context->position + offset;
                        break;
                    case SEEK_END:
                        new_pos = vh->node->size + offset;
                        break;
                    default:
                        return -EINVAL;
                }

                if (new_pos < 0) return -EINVAL;

                vh->context->position = new_pos;
                return new_pos;
            }

            default:
                return -EBADH;
        }
    }
}  // namespace syscalls::internal
