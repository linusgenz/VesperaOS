#ifndef VESPERAOS_LIBM_H
#define VESPERAOS_LIBM_H

#include <klib/string.h>
#include <vespera/types.h>

extern "C" {

typedef float  float_t;
typedef double double_t;

#define WANT_ROUNDING 1
#define issignaling_inline(x) 0

static inline double eval_as_double(double x)
{
    double y = x;
    return y;
}

#ifndef fp_barrier
#define fp_barrier fp_barrier
static inline double fp_barrier(double x)
{
    volatile double y = x;
    return y;
}
#endif

/* fp_force_eval ensures that the input value is computed when that's
   otherwise unused.  To prevent the constant folding of the input
   expression, an additional fp_barrier may be needed or a compilation
   mode that does so (e.g. -frounding-math in gcc). Then it can be
   used to evaluate an expression for its fenv side-effects only.   */

#ifndef fp_force_evalf
#define fp_force_evalf fp_force_evalf
static inline void fp_force_evalf(float x)
{
    volatile float y;
    y = x;
}
#endif

#ifndef fp_force_eval
#define fp_force_eval fp_force_eval
static inline void fp_force_eval(double x)
{
    volatile double y;
    y = x;
}
#endif

#ifndef fp_force_evall
#define fp_force_evall fp_force_evall
static inline void fp_force_evall(long double x)
{
    volatile long double y;
    y = x;
}
#endif

#define FORCE_EVAL(x) do {                        \
if (sizeof(x) == sizeof(float)) {         \
fp_force_evalf(x);                \
} else if (sizeof(x) == sizeof(double)) { \
fp_force_eval(x);                 \
} else {                                  \
fp_force_evall(x);                \
}                                         \
} while(0)

static inline uint64_t asuint64(double d) {
    uint64_t u;
    memcpy(&u, &d, sizeof(u));
    return u;
}

static inline double asdouble(uint64_t u) {
    double d;
    memcpy(&d, &u, sizeof(d));
    return d;
}

#define GET_HIGH_WORD(hi,d) (hi) = asuint64(d) >> 32
#define GET_LOW_WORD(lo,d)  (lo) = (uint32_t)asuint64(d)

#define INSERT_WORDS(d, hi, lo) \
(d) = asdouble(((uint64_t)(hi) << 32) | (uint32_t)(lo))
#define SET_LOW_WORD(d, lo) \
INSERT_WORDS(d, asuint64(d) >> 32, lo)

double __cos(double x, double y);
double __sin(double x, double y, int iy);

int __rem_pio2(double x, double *y);
int __rem_pio2_large(double *x, double *y, int e0, int nx, int prec);

double __math_xflow(uint32_t sign, double y);
double __math_uflow(uint32_t sign);
double __math_oflow(uint32_t sign);

double __math_invalid(double x);

#ifdef __GNUC__
#define predict_true(x) __builtin_expect(!!(x), 1)
#define predict_false(x) __builtin_expect(x, 0)
#else
#define predict_true(x) (x)
#define predict_false(x) (x)
#endif

#ifdef __cplusplus
    }
#endif

#endif  // VESPERAOS_LIBM_H
