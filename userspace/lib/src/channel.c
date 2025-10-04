// channel.c
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 01.10.25.
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

#include <channel.h>

CHANNEL_HANDLE channel_create(size_t capacity) {
    return sys_channel_create(capacity, 0,0,0,0,0);
}

ssize_t channel_recv(CHANNEL_HANDLE hid, void* buf, size_t len) {
    return sys_channel_recv(hid, (uint64_t)buf, len,0,0,0);
}

ssize_t channel_send(CHANNEL_HANDLE hid, const void* data, size_t len) {
    return sys_channel_send(hid, (uint64_t)data, len,0,0,0);
}