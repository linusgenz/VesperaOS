/**
 * @file framebuffer_device.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 30.12.25.
 *
 * This file is part of VesperaOS.
 *
 * VesperaOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * VesperaOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef VESPERAOS_FRAMEBUFFER_DEVICE_H
#define VESPERAOS_FRAMEBUFFER_DEVICE_H

#include "../types/handle.h"
#include "display_manager.h"

#include <kernel/devices/char_device.h>

struct FbInfo {
  uint32_t width;
  uint32_t height;
  uint32_t bpp;
  uint32_t pitch;
  uint32_t is_primary; // 1 = yes, 0 = no
};

#define FB_IOCTL_GET_INFO 0x4600
#define FB_IOCTL_GET_BACKING_DEVID 0x4601

class FramebufferDevice final : public CharDevice {
public:
  FramebufferDevice(const char *name, BusType bus) : CharDevice(name, bus) {}

  int open(CharFile **out_cf) override;
  int release(CharFile *cf) override;

  ssize_t read(CharFile *cf, void *buffer, size_t count,
               size_t offset) override;
  ssize_t write(CharFile *cf, const void *buffer, size_t count) override;

  int ioctl(CharFile *cf, uint32_t cmd, void *arg) override;
};

#endif // VESPERAOS_FRAMEBUFFER_DEVICE_H
