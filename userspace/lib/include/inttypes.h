// inttypes.h
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 26.05.26.
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

#ifndef _INTTYPES_H
#define _INTTYPES_H

#include <stdint.h>   /* provides intN_t / uintN_t, intmax_t, etc. */

#ifndef ____gwchar_t_defined
# ifdef __cplusplus
#  define __gwchar_t wchar_t
# elif defined __WCHAR_TYPE__
typedef __WCHAR_TYPE__ __gwchar_t;
# else
#  define __need_wchar_t
#  include <stddef.h>
typedef wchar_t __gwchar_t;
# endif
# define ____gwchar_t_defined	1
#endif

#define __VESP_LEN_8   "hh"
#define __VESP_LEN_16  "h"
#define __VESP_LEN_32  ""
#define __VESP_LEN_64  "l"
#define __VESP_LEN_I(w) __VESP_LEN_##w
#define __VESP_LEN(w)   __VESP_LEN_I(w)   /* expands w first, then pastes */


/* ---- fprintf ---- */
#define PRId8        __VESP_LEN_8 "d"
#define PRIi8        __VESP_LEN_8 "i"
#define PRIo8        __VESP_LEN_8 "o"
#define PRIu8        __VESP_LEN_8 "u"
#define PRIx8        __VESP_LEN_8 "x"
#define PRIX8        __VESP_LEN_8 "X"

#define PRId16       __VESP_LEN_16 "d"
#define PRIi16       __VESP_LEN_16 "i"
#define PRIo16       __VESP_LEN_16 "o"
#define PRIu16       __VESP_LEN_16 "u"
#define PRIx16       __VESP_LEN_16 "x"
#define PRIX16       __VESP_LEN_16 "X"

#define PRId32       __VESP_LEN_32 "d"
#define PRIi32       __VESP_LEN_32 "i"
#define PRIo32       __VESP_LEN_32 "o"
#define PRIu32       __VESP_LEN_32 "u"
#define PRIx32       __VESP_LEN_32 "x"
#define PRIX32       __VESP_LEN_32 "X"

#define PRId64       __VESP_LEN_64 "d"
#define PRIi64       __VESP_LEN_64 "i"
#define PRIo64       __VESP_LEN_64 "o"
#define PRIu64       __VESP_LEN_64 "u"
#define PRIx64       __VESP_LEN_64 "x"
#define PRIX64       __VESP_LEN_64 "X"

#define PRIdLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "d"
#define PRIiLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "i"
#define PRIoLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "o"
#define PRIuLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "u"
#define PRIxLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "x"
#define PRIXLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "X"

#define PRIdLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "d"
#define PRIiLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "i"
#define PRIoLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "o"
#define PRIuLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "u"
#define PRIxLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "x"
#define PRIXLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "X"

#define PRIdLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "d"
#define PRIiLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "i"
#define PRIoLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "o"
#define PRIuLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "u"
#define PRIxLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "x"
#define PRIXLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "X"

#define PRIdLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "d"
#define PRIiLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "i"
#define PRIoLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "o"
#define PRIuLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "u"
#define PRIxLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "x"
#define PRIXLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "X"

#define PRIdFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "d"
#define PRIiFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "i"
#define PRIoFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "o"
#define PRIuFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "u"
#define PRIxFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "x"
#define PRIXFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "X"

#define PRIdFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "d"
#define PRIiFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "i"
#define PRIoFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "o"
#define PRIuFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "u"
#define PRIxFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "x"
#define PRIXFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "X"

#define PRIdFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "d"
#define PRIiFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "i"
#define PRIoFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "o"
#define PRIuFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "u"
#define PRIxFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "x"
#define PRIXFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "X"

#define PRIdFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "d"
#define PRIiFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "i"
#define PRIoFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "o"
#define PRIuFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "u"
#define PRIxFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "x"
#define PRIXFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "X"

#define PRIdMAX      __VESP_LEN(__INTMAX_WIDTH__) "d"
#define PRIiMAX      __VESP_LEN(__INTMAX_WIDTH__) "i"
#define PRIoMAX      __VESP_LEN(__INTMAX_WIDTH__) "o"
#define PRIuMAX      __VESP_LEN(__INTMAX_WIDTH__) "u"
#define PRIxMAX      __VESP_LEN(__INTMAX_WIDTH__) "x"
#define PRIXMAX      __VESP_LEN(__INTMAX_WIDTH__) "X"

#define PRIdPTR      __VESP_LEN(__INTPTR_WIDTH__) "d"
#define PRIiPTR      __VESP_LEN(__INTPTR_WIDTH__) "i"
#define PRIoPTR      __VESP_LEN(__INTPTR_WIDTH__) "o"
#define PRIuPTR      __VESP_LEN(__INTPTR_WIDTH__) "u"
#define PRIxPTR      __VESP_LEN(__INTPTR_WIDTH__) "x"
#define PRIXPTR      __VESP_LEN(__INTPTR_WIDTH__) "X"

/* ---- fscanf (note: scanf has no 'hh'-less exact-width quirks; same modifiers) ---- */
#define SCNd8        __VESP_LEN_8 "d"
#define SCNi8        __VESP_LEN_8 "i"
#define SCNo8        __VESP_LEN_8 "o"
#define SCNu8        __VESP_LEN_8 "u"
#define SCNx8        __VESP_LEN_8 "x"

#define SCNd16       __VESP_LEN_16 "d"
#define SCNi16       __VESP_LEN_16 "i"
#define SCNo16       __VESP_LEN_16 "o"
#define SCNu16       __VESP_LEN_16 "u"
#define SCNx16       __VESP_LEN_16 "x"

#define SCNd32       __VESP_LEN_32 "d"
#define SCNi32       __VESP_LEN_32 "i"
#define SCNo32       __VESP_LEN_32 "o"
#define SCNu32       __VESP_LEN_32 "u"
#define SCNx32       __VESP_LEN_32 "x"

#define SCNd64       __VESP_LEN_64 "d"
#define SCNi64       __VESP_LEN_64 "i"
#define SCNo64       __VESP_LEN_64 "o"
#define SCNu64       __VESP_LEN_64 "u"
#define SCNx64       __VESP_LEN_64 "x"

#define SCNdLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "d"
#define SCNiLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "i"
#define SCNoLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "o"
#define SCNuLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "u"
#define SCNxLEAST8   __VESP_LEN(__INT_LEAST8_WIDTH__) "x"

#define SCNdLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "d"
#define SCNiLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "i"
#define SCNoLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "o"
#define SCNuLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "u"
#define SCNxLEAST16  __VESP_LEN(__INT_LEAST16_WIDTH__) "x"

#define SCNdLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "d"
#define SCNiLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "i"
#define SCNoLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "o"
#define SCNuLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "u"
#define SCNxLEAST32  __VESP_LEN(__INT_LEAST32_WIDTH__) "x"

#define SCNdLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "d"
#define SCNiLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "i"
#define SCNoLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "o"
#define SCNuLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "u"
#define SCNxLEAST64  __VESP_LEN(__INT_LEAST64_WIDTH__) "x"

#define SCNdFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "d"
#define SCNiFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "i"
#define SCNoFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "o"
#define SCNuFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "u"
#define SCNxFAST8    __VESP_LEN(__INT_FAST8_WIDTH__) "x"

#define SCNdFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "d"
#define SCNiFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "i"
#define SCNoFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "o"
#define SCNuFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "u"
#define SCNxFAST16   __VESP_LEN(__INT_FAST16_WIDTH__) "x"

#define SCNdFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "d"
#define SCNiFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "i"
#define SCNoFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "o"
#define SCNuFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "u"
#define SCNxFAST32   __VESP_LEN(__INT_FAST32_WIDTH__) "x"

#define SCNdFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "d"
#define SCNiFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "i"
#define SCNoFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "o"
#define SCNuFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "u"
#define SCNxFAST64   __VESP_LEN(__INT_FAST64_WIDTH__) "x"

#define SCNdMAX      __VESP_LEN(__INTMAX_WIDTH__) "d"
#define SCNiMAX      __VESP_LEN(__INTMAX_WIDTH__) "i"
#define SCNoMAX      __VESP_LEN(__INTMAX_WIDTH__) "o"
#define SCNuMAX      __VESP_LEN(__INTMAX_WIDTH__) "u"
#define SCNxMAX      __VESP_LEN(__INTMAX_WIDTH__) "x"

#define SCNdPTR      __VESP_LEN(__INTPTR_WIDTH__) "d"
#define SCNiPTR      __VESP_LEN(__INTPTR_WIDTH__) "i"
#define SCNoPTR      __VESP_LEN(__INTPTR_WIDTH__) "o"
#define SCNuPTR      __VESP_LEN(__INTPTR_WIDTH__) "u"
#define SCNxPTR      __VESP_LEN(__INTPTR_WIDTH__) "x"


/* =========================================================================
 * §7.8.2  imaxdiv_t
 * ========================================================================= */

typedef struct {
    intmax_t quot; /* quotient  */
    intmax_t rem;  /* remainder */
} imaxdiv_t;

/* =========================================================================
 * §7.8.2  Function declarations
 *
 * Implementations belong in your libc; declarations only here.
 * ========================================================================= */

#ifdef __cplusplus
extern "C" {

#endif

/**
 * @brief  Compute the absolute value of j.
 * @return |j|; behaviour is undefined if the result cannot be represented.
 */
intmax_t imaxabs(intmax_t j);

/**
 * @brief  Compute the quotient and remainder of numer / denom.
 * @return imaxdiv_t with .quot and .rem set.
 */
imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

/**
 * @brief  Convert the initial portion of nptr to intmax_t.
 *         Identical in behaviour to strtoll() but for intmax_t.
 */
intmax_t strtoimax(
    const char* __restrict nptr,
    char** __restrict endptr,
    int base
);

/**
 * @brief  Convert the initial portion of nptr to uintmax_t.
 *         Identical in behaviour to strtoull() but for uintmax_t.
 */
uintmax_t strtoumax(
    const char* __restrict nptr,
    char** __restrict endptr,
    int base
);

/*
 * wcstoimax / wcstoumax are part of C11 §7.8.2 but require <wchar.h> /
 * wchar_t.  Declare them only when wchar_t is available.  VesperaOS does
 * not currently provide a wchar layer, so they are guarded here.
 */
#ifdef __WCHAR_TYPE__

intmax_t wcstoimax(
    const __gwchar_t* __restrict nptr,
    __gwchar_t** __restrict endptr,
    int base
);

uintmax_t wcstoumax(
    const __gwchar_t* __restrict nptr,
    __gwchar_t** __restrict endptr,
    int base
);
#endif /* __WCHAR_TYPE__ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _INTTYPES_H */
