/**
 * @file ctype.h
 * VesperaOS - operating system for the x86_64 architecture
 *
 * Copyright (c) 2025 Linus Genz <mail@linusgenz.dev>
 *
 * Created by Linus Genz on 02.01.26.
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
#ifndef _CTYPE_H
#define _CTYPE_H

#include <stdint.h>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
# define _ISbit(bit) (1 << (bit))
#else
# define _ISbit(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
#endif

enum {
    _ISupper  = _ISbit(0),  /* UPPERCASE */
    _ISlower  = _ISbit(1),  /* lowercase */
    _ISalpha  = _ISbit(2),  /* Alphabetic */
    _ISdigit  = _ISbit(3),  /* Numeric */
    _ISxdigit = _ISbit(4),  /* Hexadecimal numeric */
    _ISspace  = _ISbit(5),  /* Whitespace */
    _ISprint  = _ISbit(6),  /* Printing */
    _ISgraph  = _ISbit(7),  /* Graphical */
    _ISblank  = _ISbit(8),  /* Blank (usually SPC and TAB) */
    _IScntrl  = _ISbit(9),  /* Control character */
    _ISpunct  = _ISbit(10), /* Punctuation */
    _ISalnum  = _ISbit(11)  /* Alphanumeric */
};

/* Classification table: 768 bytes (384 uint16_t entries)
   Indexed from -128 to 255, allowing negative signed char values
   The actual table starts at index 0, use ctype_b + 128 for indexing */
extern const uint16_t _ctype_b[384];

/* Case conversion tables: 384 uint32_t entries each
   Same indexing as _ctype_b: use ctype_tolower/toupper + 128 */
extern const uint32_t _ctype_toupper[384];
extern const uint32_t _ctype_tolower[384];

/* Wide character classification tables (bit arrays for wctype) */
extern const uint32_t _ctype_class_upper[2];
extern const uint32_t _ctype_class_lower[2];
extern const uint32_t _ctype_class_alpha[2];
extern const uint32_t _ctype_class_digit[2];
extern const uint32_t _ctype_class_xdigit[4];
extern const uint32_t _ctype_class_space[2];
extern const uint32_t _ctype_class_print[4];
extern const uint32_t _ctype_class_graph[4];
extern const uint32_t _ctype_class_blank[2];
extern const uint32_t _ctype_class_cntrl[4];
extern const uint32_t _ctype_class_punct[4];
extern const uint32_t _ctype_class_alnum[4];


/* Macro implementations for performance */
#ifndef _NO_CTYPE_MACROS

/* Offset of 128 allows indexing with negative values */
#define __ctype_b_loc()      (_ctype_b + 128)
#define __ctype_toupper_loc() (_ctype_toupper + 128)
#define __ctype_tolower_loc() (_ctype_tolower + 128)

#define __isctype(c, type) (__ctype_b_loc()[(int)(c)] & (uint16_t)(type))

#define isalnum(c)  __isctype((c), _ISalnum)
#define isalpha(c)  __isctype((c), _ISalpha)
#define isblank(c)  __isctype((c), _ISblank)
#define iscntrl(c)  __isctype((c), _IScntrl)
#define isdigit(c)  __isctype((c), _ISdigit)
#define isgraph(c)  __isctype((c), _ISgraph)
#define islower(c)  __isctype((c), _ISlower)
#define isprint(c)  __isctype((c), _ISprint)
#define ispunct(c)  __isctype((c), _ISpunct)
#define isspace(c)  __isctype((c), _ISspace)
#define isupper(c)  __isctype((c), _ISupper)
#define isxdigit(c) __isctype((c), _ISxdigit)

#define tolower(c) ((c) >= -128 && (c) < 256 ? __ctype_tolower_loc()[(c)] : (c))
#define toupper(c) ((c) >= -128 && (c) < 256 ? __ctype_toupper_loc()[(c)] : (c))

#endif /* _NO_CTYPE_MACROS */


#endif //VESPERAOS_CTYPE_H