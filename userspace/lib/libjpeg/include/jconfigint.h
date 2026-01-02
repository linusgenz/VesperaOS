/**
 * @file jconfigint.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 01.01.26.
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
#ifndef VESPERAOS_JCONFIGINT_H
#define VESPERAOS_JCONFIGINT_H

#define HIDDEN  __attribute__((visibility("hidden")))

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#define INLINE  __inline__ __attribute__((always_inline))

#define THREAD_LOCAL /* __thread */

#define SIZEOF_SIZE_T  8

#undef C_ARITH_CODING_SUPPORTED

#undef RIGHT_SHIFT_IS_UNSIGNED
#undef NEED_FAR_POINTERS
#undef HAVE_PROTOTYPES
#undef HAVE_UNSIGNED_CHAR
#undef HAVE_UNSIGNED_SHORT

#if defined(__has_attribute)
#if __has_attribute(fallthrough)
#define FALLTHROUGH  __attribute__((fallthrough));
#else
#define FALLTHROUGH
#endif
#else
#define FALLTHROUGH
#endif

/*
 * Define BITS_IN_JSAMPLE as either
 *   8   for 8-bit sample values (the usual setting)
 *   12  for 12-bit sample values
 * Only 8 and 12 are legal data precisions for lossy JPEG according to the
 * JPEG standard, and the IJG code does not support anything else!
 */

#define VERSION  "3.1.3"
#define PACKAGE_NAME  "libjpeg-turbo"
#define BUILD BUILD_DEF


#define BITS_IN_JSAMPLE  8

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED

#endif //VESPERAOS_JCONFIGINT_H