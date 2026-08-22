// ioctl.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 02.04.26.
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
#ifndef VESPERAOS_IOCTL_H
#define VESPERAOS_IOCTL_H

#define IOC_DIR_BITS   2
#define IOC_SIZE_BITS   14
#define IOC_TYPE_BITS   8
#define IOC_NR_BITS     8

#define IOC_DIR_SHIFT   30
#define IOC_SIZE_SHIFT  16
#define IOC_TYPE_SHIFT  8
#define IOC_NR_SHIFT    0

/* Direction flags */
#define IOC_NONE  0
#define IOC_WRITE 1
#define IOC_READ  2

/* Bit masks */
#define IOC_DIR_MASK  ((1u << IOC_DIR_BITS) - 1)
#define IOC_SIZE_MASK ((1u << IOC_SIZE_BITS) - 1)
#define IOC_TYPE_MASK ((1u << IOC_TYPE_BITS) - 1)
#define IOC_NR_MASK   ((1u << IOC_NR_BITS) - 1)

#define IOC(dir, type, nr, size) \
(((dir & IOC_DIR_MASK)  << IOC_DIR_SHIFT)  | \
((size & IOC_SIZE_MASK) << IOC_SIZE_SHIFT) | \
((type & IOC_TYPE_MASK) << IOC_TYPE_SHIFT) | \
((nr   & IOC_NR_MASK)   << IOC_NR_SHIFT))

#define IO(type, nr) \
IOC(IOC_NONE, (type), (nr), 0)

#define IOR(type, nr, data_type) \
IOC(IOC_READ, (type), (nr), sizeof(data_type))

#define IOW(type, nr, data_type) \
IOC(IOC_WRITE, (type), (nr), sizeof(data_type))

#define IOWR(type, nr, data_type) \
IOC(IOC_READ | IOC_WRITE, (type), (nr), sizeof(data_type))


#define _IO(type, nr)                   IO(type, nr)
#define _IOR(type, nr, data_type)       IOR(type, nr, data_type)
#define _IOW(type, nr, data_type)       IOW(type, nr, data_type)
#define _IOWR(type, nr, data_type)      IOWR(type, nr, data_type)


/* Decoder */
#define IOC_GET_DIR(cmd)  ((uint32_t)((cmd >> IOC_DIR_SHIFT) & IOC_DIR_MASK))
#define IOC_GET_SIZE(cmd) ((uint32_t)((cmd >> IOC_SIZE_SHIFT) & IOC_SIZE_MASK))
#define IOC_GET_TYPE(cmd) ((uint32_t)((cmd >> IOC_TYPE_SHIFT) & IOC_TYPE_MASK))
#define IOC_GET_NR(cmd)   ((uint32_t)((cmd >> IOC_NR_SHIFT) & IOC_NR_MASK))

#endif  // VESPERAOS_IOCTL_H
