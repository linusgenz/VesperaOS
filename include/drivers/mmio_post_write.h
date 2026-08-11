// mmio_post_write.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.08.26.
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
#ifndef VESPERAOS_MMIO_POST_WRITE_H
#define VESPERAOS_MMIO_POST_WRITE_H

#define MMIO_POST_WRITE(reg)             \
    do {                                 \
        volatile u32 __tmp = (reg).raw; \
        (void)__tmp;                     \
    } while (0)

#define MMIO_POST_WRITE64(reg)           \
    do {                                 \
        volatile u64 __tmp = (reg).raw; \
        (void)__tmp;                     \
    } while (0)

#define MMIO_POST_WRITE_PTR(reg)             \
    do {                                 \
        volatile u32 __tmp = (reg)->raw; \
        (void)__tmp;                     \
    } while (0)

#define MMIO_POST_WRITE64_PTR(reg)       \
    do {                                 \
        volatile u64 __tmp = (reg)->raw;  \
        (void)__tmp;                     \
    } while (0)

#endif //VESPERAOS_MMIO_POST_WRITE_H
