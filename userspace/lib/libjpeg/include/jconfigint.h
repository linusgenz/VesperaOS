/*
 * This header is based on jconfigint.h.in from libjpeg-turbo
 **/
#ifndef JCONFIGINT_H
#define JCONFIGINT_H

/* Set via cmake */
#define BUILD BUILD_DEF

/* How to hide global symbols. */
#define HIDDEN  __attribute__((visibility("hidden")))

/* Compiler's inline keyword */
#undef inline

/* How to obtain function inlining. */
#define INLINE  __inline__ __attribute__((always_inline))

/* How to obtain thread-local storage */
#define THREAD_LOCAL /* __thread */ // TODO Not implemented yet

/* Define to the full name of this package. */
#define PACKAGE_NAME  "libjpeg-turbo"

/* Version number of package */
#define VERSION  "3.1.3"

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T  8

/* Define if your compiler has __builtin_ctzl() and sizeof(unsigned long) == sizeof(size_t). */
#define HAVE_BUILTIN_CTZL

#undef C_ARITH_CODING_SUPPORTED

#undef RIGHT_SHIFT_IS_UNSIGNED


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

#define BITS_IN_JSAMPLE  8

#undef C_ARITH_CODING_SUPPORTED
#undef D_ARITH_CODING_SUPPORTED
#undef WITH_SIMD

#if BITS_IN_JSAMPLE == 8

/* Support arithmetic encoding */
// #define C_ARITH_CODING_SUPPORTED 1

/* Support arithmetic decoding */
// #define D_ARITH_CODING_SUPPORTED 1

/* Use accelerated SIMD routines. */
#define WITH_SIMD 1

#endif

#endif //JCONFIGINT_H