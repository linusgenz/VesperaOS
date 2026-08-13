// dlfcn.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 12.08.26.
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

#ifndef VESPERAOS_DLFCN_H
#define VESPERAOS_DLFCN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve symbols only when first referenced (lazy binding).
 *
 * Passed as (part of) @p mode to dlopen(). Relocations for function
 * symbols are deferred until the symbol is actually used.
 */
#define RTLD_LAZY   0x0001

/**
 * @brief Resolve all symbols immediately when the library is loaded.
 *
 * Passed as (part of) @p mode to dlopen(). If any symbol cannot be
 * resolved, dlopen() fails.
 */
#define RTLD_NOW    0x0002

/**
 * @brief Keep the library's symbols local to the handle (default).
 *
 * Symbols are not made available for resolving references in
 * subsequently loaded libraries.
 */
#define RTLD_LOCAL  0x0000

/**
 * @brief Make the library's symbols available to other libraries.
 *
 * Passed as (part of) @p mode to dlopen(). Symbols become part of the
 * global symbol scope and can satisfy references in libraries loaded
 * afterwards.
 */
#define RTLD_GLOBAL 0x0004

/**
 * @brief Load a shared library and map it into the calling unit's
 *        address space.
 *
 * Parses the ELF image at @p path, maps its segments, resolves its
 * dependencies, applies relocations and (depending on @p mode) runs
 * its constructors. Repeated calls with the same @p path return the
 * same handle and increment an internal reference count.
 *
 * @param path Path to the shared library (e.g. "/lib/libm.so"). Must
 *             not be NULL.
 * @param mode Bitwise OR of exactly one of RTLD_LAZY / RTLD_NOW,
 *             optionally combined with RTLD_GLOBAL.
 * @return Opaque handle to be passed to dlsym() / dlclose() on
 *         success, or NULL on failure. Use dlerror() to retrieve the
 *         reason.
 *
 * @see dlsym()
 * @see dlclose()
 * @see dlerror()
 */
void* dlopen(const char* path, int mode);

/**
 * @brief Look up the address of a symbol in a loaded library.
 *
 * @param handle Handle returned by dlopen(). Must refer to a
 *               currently loaded library.
 * @param name   Null-terminated name of the symbol to resolve.
 * @return Address of the symbol on success, or NULL if it could not
 *         be found. Use dlerror() to distinguish "not found" from a
 *         symbol that legitimately resolves to NULL.
 *
 * @see dlopen()
 * @see dlerror()
 */
void* dlsym(void* handle, const char* name);

/**
 * @brief Release a handle previously obtained from dlopen().
 *
 * Decrements the library's internal reference count. Once it reaches
 * zero, the library's destructors are run and its mappings are
 * released. @p handle must not be used again afterwards.
 *
 * @param handle Handle returned by dlopen().
 * @return 0 on success, or a negative error code on failure.
 *
 * @see dlopen()
 */
int dlclose(void* handle);

/**
 * @brief Return a description of the most recent dlopen()/dlsym()/
 *        dlclose() failure on the calling unit.
 *
 * Each call consumes the stored error: if no error occurred since the
 * last call to dlerror(), NULL is returned.
 *
 * @return Null-terminated, human-readable error string, or NULL if
 *         there is no pending error.
 */
char* dlerror(void);

#ifdef __cplusplus
}
#endif

#endif  // VESPERAOS_DLFCN_H
