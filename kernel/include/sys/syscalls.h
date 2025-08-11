// syscalls.h
//
// VesperaOS - operating system for the x86_64 architecture
// 
// Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
// 
// Created by Linus Genz on 03.08.25.
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

#ifndef SYSCALLS_H
#define SYSCALLS_H

int64_t sys_write(int64_t fd, const void* buf, size_t size);
int64_t sys_read(int64_t fd, void* buf, size_t size);
int64_t sys_open(const char* path);
int64_t sys_close(int64_t fd);
int64_t sys_exit(int64_t code);
int64_t sys_create(const char* path);
int64_t sys_rename(const char *old_path, const char *new_path);
int64_t sys_mkdir(const char* path);
int64_t sys_rmdir(const char* path);
int64_t sys_unlink(const char* path);

#endif //SYSCALLS_H
