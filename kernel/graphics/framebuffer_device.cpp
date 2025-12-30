/**
 * @file framebuffer_device.cpp
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

#include "framebuffer_device.h"

#include <errno.h>

int FramebufferDevice::open(CharFile** out_cf) {
  if (!out_cf) return -EINVAL;

  auto* cf = new CharFile();
  cf->driver_private = nullptr;
  *out_cf = cf;
  return 0;
}

int FramebufferDevice::release(CharFile* cf) {
  delete cf;
  return 0;
}

ssize_t FramebufferDevice::read(CharFile* /*cf*/, void* /*buffer*/, size_t /*count*/, size_t /*offset*/) {
  return -EUNSUPPORTED;
}

ssize_t FramebufferDevice::write(CharFile* /*cf*/, const void* /*buffer*/, size_t /*count*/) {
  return -EUNSUPPORTED;
}

int FramebufferDevice::ioctl(CharFile* /*cf*/, uint32_t cmd, void* arg) {
  if (!arg) return -EINVAL;

  auto backend = DisplayManager::primary();
  if (!backend.drv) return -ENODEV;

  switch (cmd) {
  case FB_IOCTL_GET_INFO: {
    auto* info = static_cast<FbInfo*>(arg);
    info->width  = backend.drv->screen_width_px();
    info->height = backend.drv->screen_height_px();
    info->bpp    = 32;
    info->pitch  = 0;
    info->is_primary = 1;
    return 0;
  }
  case FB_IOCTL_GET_BACKING_DEVID:
    *static_cast<uint32_t*>(arg) = backend.kd ? backend.kd->id : 0;
    return 0;
  default:
    return -ENOTTY;
  }
}