// dirent.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 22.08.26.
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


#include <dirent.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sysstd.h>
#include <unistd.h>

#include <vespera/handles.h>
#include "internal/dirstream.h"
#include "internal/fd_table.h"

static DIR* dirstream_alloc(int fd, const char* path) {
    DIR* dirp = (DIR*)malloc(sizeof(DIR));
    if (dirp == (DIR*)0) {
        errno = ENOMEM;
        return (DIR*)0;
    }

    dirp->fd = fd;
    dirp->at_eof = 0;
    memset(&dirp->entry, 0, sizeof(dirent_t));

    if (path != (const char*)0) {
        size_t len = strlen(path) + 1;
        dirp->path = (char*)malloc(len);
        if (dirp->path == (char*)0) {
            dirp->path = (char*)0;
        } else {
            memcpy(dirp->path, path, len);
        }
    } else {
        dirp->path = (char*)0;
    }

    return dirp;
}

DIR* opendir(const char* path) {
    if (path == (const char*)0) {
        errno = EINVAL;
        return (DIR*)0;
    }

    int64_t ret = sys_open((uint64_t)(uintptr_t)path, (uint64_t)(O_RDONLY | O_DIRECTORY), 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return (DIR*)0;
    }

    FILE_HANDLE handle = (FILE_HANDLE)ret;
    int fd = fd_table_insert(handle);
    if (fd < 0) {
        sys_close((uint64_t)handle, 0, 0, 0, 0, 0);
        errno = EMFILE;
        return (DIR*)0;
    }

    DIR* dirp = dirstream_alloc(fd, path);
    if (dirp == (DIR*)0) {
        // dirstream_alloc() already set errno.
        fd_table_remove(fd);
        sys_close((uint64_t)handle, 0, 0, 0, 0, 0);
        return (DIR*)0;
    }

    return dirp;
}

DIR* fdopendir(int fd) {
    if (!fd_table_valid(fd)) {
        errno = EBADH;
        return (DIR*)0;
    }

    // path is NULL: rewinddir() will be a no-op for streams opened this
    // way, since we don't know what path the fd was opened from.
    DIR* dirp = dirstream_alloc(fd, (const char*)0);
    if (dirp == (DIR*)0) {
        return (DIR*)0;
    }

    return dirp;
}

dirent_t* readdir(DIR* dirp) {
    if (dirp == (DIR*)0) {
        errno = EBADH;
        return (dirent_t*)0;
    }

    if (dirp->at_eof) {
        return (dirent_t*)0;
    }

    FILE_HANDLE handle = fd_table_get(dirp->fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return (dirent_t*)0;
    }

    int64_t ret = sys_readdir((uint64_t)handle, (uint64_t)(uintptr_t)&dirp->entry, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return (dirent_t*)0;
    }
    if (ret == 0) {
        dirp->at_eof = 1;
        return (dirent_t*)0;
    }

    return &dirp->entry;
}

int closedir(DIR* dirp) {
    if (dirp == (DIR*)0) {
        errno = EBADH;
        return -1;
    }

    FILE_HANDLE handle = fd_table_get(dirp->fd);
    if (handle == INVALID_HANDLE) {
        errno = EBADH;
        return -1;
    }

    int64_t ret = sys_close((uint64_t)handle, 0, 0, 0, 0, 0);
    if (ret < 0) {
        errno = (int)(-ret);
        return -1;
    }

    fd_table_remove(dirp->fd);

    if (dirp->path != (char*)0) {
        free(dirp->path);
    }
    free(dirp);
    return 0;
}

void rewinddir(DIR* dirp) {
    if (dirp == (DIR*)0) {
        errno = EBADH;
        return;
    }

    if (dirp->path == (char*)0) {
        errno = EBADH;
        return;
    }

    FILE_HANDLE old_handle = fd_table_get(dirp->fd);

    int64_t open_ret = sys_open((uint64_t)(uintptr_t)dirp->path, (uint64_t)(O_RDONLY | O_DIRECTORY), 0, 0, 0, 0);
    if (open_ret < 0) {
        errno = (int)(-open_ret);
        return;
    }
    FILE_HANDLE new_handle = (FILE_HANDLE)open_ret;

    fd_table_remove(dirp->fd);
    int new_fd = fd_table_insert(new_handle);
    if (new_fd < 0) {
        sys_close((uint64_t)new_handle, 0, 0, 0, 0, 0);
        errno = EMFILE;
        dirp->fd = -1;
        return;
    }

    if (old_handle != INVALID_HANDLE) {
        sys_close((uint64_t)old_handle, 0, 0, 0, 0, 0);
    }

    dirp->fd = new_fd;
    dirp->at_eof = 0;
    memset(&dirp->entry, 0, sizeof(dirent_t));
}

int dirfd(DIR* dirp) {
    if (dirp == (DIR*)0) {
        errno = EBADH;
        return -1;
    }
    return dirp->fd;
}

