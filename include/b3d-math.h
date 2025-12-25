/*
 * B3D Math Utilities
 *
 * Unified interface for float math operations. These work in both
 * fixed-point mode (default) and floating-point mode (B3D_FLOAT_POINT).
 * Use these instead of raw sinf/cosf/etc. for consistency across all
 * B3D examples.
 */

#ifndef B3D_PUBLIC_MATH_H
#define B3D_PUBLIC_MATH_H

#include <limits.h>
#include <math.h>
#include <stdint.h>

#ifndef B3D_MATH_UTILS_DEFINED
#define B3D_MATH_UTILS_DEFINED

#ifdef B3D_FLOAT_POINT

/* Floating-point mode: use standard math library */

static inline float b3d_sinf(float x)
{
    return sinf(x);
}

static inline float b3d_cosf(float x)
{
    return cosf(x);
}

static inline float b3d_tanf(float x)
{
    return tanf(x);
}

static inline float b3d_sqrtf(float x)
{
    return x <= 0.0f ? 0.0f : sqrtf(x);
}

static inline float b3d_fabsf(float x)
{
    return x < 0.0f ? -x : x;
}

/* Compute sine and cosine simultaneously (more efficient than separate calls)
 * @x: angle in radians
 * @sinp: pointer to store sine result
 * @cosp: pointer to store cosine result
 */
static inline void b3d_sincosf(float x, float *sinp, float *cosp)
{
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    sincosf(x, sinp, cosp);
#else
    *sinp = sinf(x);
    *cosp = cosf(x);
#endif
}

#else /* Fixed-point mode (default) */

/*
 * Q15.16 fixed-point implementation using Bhaskara I approximation.
 * Provides ~0.3% max error for sine/cosine without requiring FPU.
 */

typedef int32_t b3d_math_fixed_t;

#define B3D_MATH_FP_BITS 16
#define B3D_MATH_FP_ONE (1LL << B3D_MATH_FP_BITS)

#define B3D_MATH_FLOAT_TO_FP(f) ((b3d_math_fixed_t) ((f) * B3D_MATH_FP_ONE))
#define B3D_MATH_FP_TO_FLOAT(f) ((float) (f) / B3D_MATH_FP_ONE)
#define B3D_MATH_FP_MUL(a, b) \
    ((b3d_math_fixed_t) (((int64_t) (a) * (b)) >> B3D_MATH_FP_BITS))
#define B3D_MATH_FP_DIV(a, b) \
    ((b) == 0 ? 0             \
              : (b3d_math_fixed_t) (((int64_t) (a) * B3D_MATH_FP_ONE) / (b)))

/* Fixed-point pi constants using rational approximation 355/113 */
#define B3D_MATH_FP_PI ((b3d_math_fixed_t) ((355LL << B3D_MATH_FP_BITS) / 113))
#define B3D_MATH_FP_PI_HALF (B3D_MATH_FP_PI >> 1)
#define B3D_MATH_FP_3PI_HALF (B3D_MATH_FP_PI + B3D_MATH_FP_PI_HALF)
#define B3D_MATH_FP_2PI (B3D_MATH_FP_PI << 1)
#define B3D_MATH_FP_PI_SQ                                               \
    ((b3d_math_fixed_t) (((int64_t) B3D_MATH_FP_PI * B3D_MATH_FP_PI) >> \
                         B3D_MATH_FP_BITS))

/* Bhaskara I kernel for x in [0, π], returns positive sine approximation */
static inline b3d_math_fixed_t b3d_math_fp_sin_core(b3d_math_fixed_t x)
{
    b3d_math_fixed_t xp = B3D_MATH_FP_MUL(x, B3D_MATH_FP_PI - x);
    b3d_math_fixed_t denom = 5 * B3D_MATH_FP_PI_SQ - 4 * xp;
    if (denom == 0)
        return 0;
    return B3D_MATH_FP_DIV(16 * xp, denom);
}

/* Bhaskara I sine: sin(x) ≈ 16x(π-x) / (5π² - 4x(π-x)), ~0.3% max error */
static inline b3d_math_fixed_t b3d_math_fp_sin(b3d_math_fixed_t x)
{
    int sign = 1;
    int64_t x64 = x;

    /* Handle negative angles - guard against INT32_MIN overflow */
    if (x64 < 0) {
        if (x64 == INT32_MIN)
            x64 = INT32_MAX;
        else
            x64 = -x64;
        sign = -sign;
    }

    /* Fast modulo reduction */
    if (x64 >= B3D_MATH_FP_2PI)
        x64 %= (int64_t) B3D_MATH_FP_2PI;

    /* Map (π, 2π) to (0, π) with sign flip */
    b3d_math_fixed_t angle = (b3d_math_fixed_t) x64;
    if (angle > B3D_MATH_FP_PI) {
        angle -= B3D_MATH_FP_PI;
        sign = -sign;
    }

    return sign * b3d_math_fp_sin_core(angle);
}

/* Compute sine and cosine together to share reduction work */
static inline void b3d_math_fp_sincos(b3d_math_fixed_t x,
                                      b3d_math_fixed_t *sinp,
                                      b3d_math_fixed_t *cosp)
{
    int sin_sign = 1;
    int64_t x64 = x;

    if (x64 < 0) {
        if (x64 == INT32_MIN)
            x64 = INT32_MAX;
        else
            x64 = -x64;
        sin_sign = -1;
    }

    if (x64 >= B3D_MATH_FP_2PI)
        x64 %= (int64_t) B3D_MATH_FP_2PI;

    b3d_math_fixed_t angle = (b3d_math_fixed_t) x64;

    /* Sine computation */
    b3d_math_fixed_t sin_angle = angle;
    if (sin_angle > B3D_MATH_FP_PI) {
        sin_angle -= B3D_MATH_FP_PI;
        sin_sign = -sin_sign;
    }
    b3d_math_fixed_t sin_val = b3d_math_fp_sin_core(sin_angle);
    if (sinp)
        *sinp = (sin_sign == 1) ? sin_val : -sin_val;

    /* Cosine via quadrant mapping to [0, π/2] */
    int quadrant = (int) (x64 / (int64_t) B3D_MATH_FP_PI_HALF);
    b3d_math_fixed_t cos_angle;
    int cos_sign;

    switch (quadrant) {
    case 0:
        cos_angle = B3D_MATH_FP_PI_HALF - angle;
        cos_sign = 1;
        break;
    case 1:
        cos_angle = angle - B3D_MATH_FP_PI_HALF;
        cos_sign = -1;
        break;
    case 2:
        cos_angle = B3D_MATH_FP_3PI_HALF - angle;
        cos_sign = -1;
        break;
    default:
        cos_angle = angle - B3D_MATH_FP_3PI_HALF;
        cos_sign = 1;
        break;
    }

    b3d_math_fixed_t cos_val = b3d_math_fp_sin_core(cos_angle);
    if (cosp)
        *cosp = (cos_sign == 1) ? cos_val : -cos_val;
}

static inline b3d_math_fixed_t b3d_math_fp_cos(b3d_math_fixed_t x)
{
    int64_t x64 = (int64_t) x + (int64_t) B3D_MATH_FP_PI_HALF;
    if (x64 > INT32_MAX)
        x64 = x64 % (int64_t) B3D_MATH_FP_2PI;
    else if (x64 < INT32_MIN)
        x64 = -((-x64) % (int64_t) B3D_MATH_FP_2PI);
    return b3d_math_fp_sin((b3d_math_fixed_t) x64);
}

/* Integer sqrt on Q16.16: computes floor(sqrt(a)) in fixed-point */
static inline b3d_math_fixed_t b3d_math_fp_sqrt(b3d_math_fixed_t a)
{
    if (a <= 0)
        return 0;

    uint64_t n = ((uint64_t) (uint32_t) a) << B3D_MATH_FP_BITS;
    uint64_t res = 0;
    uint64_t bit = 1ULL << 62;

    while (bit > n)
        bit >>= 2;

    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    return res > INT32_MAX ? INT32_MAX : (b3d_math_fixed_t) res;
}

/* Public float wrappers using fixed-point internally */

static inline float b3d_sinf(float x)
{
    return B3D_MATH_FP_TO_FLOAT(b3d_math_fp_sin(B3D_MATH_FLOAT_TO_FP(x)));
}

static inline float b3d_cosf(float x)
{
    return B3D_MATH_FP_TO_FLOAT(b3d_math_fp_cos(B3D_MATH_FLOAT_TO_FP(x)));
}

static inline float b3d_tanf(float x)
{
    float c = b3d_cosf(x);
    return (c < 1e-7f && c > -1e-7f) ? 0.0f : b3d_sinf(x) / c;
}

static inline float b3d_sqrtf(float x)
{
    return B3D_MATH_FP_TO_FLOAT(b3d_math_fp_sqrt(B3D_MATH_FLOAT_TO_FP(x)));
}

static inline float b3d_fabsf(float x)
{
    return x < 0.0f ? -x : x;
}

/* Compute sine and cosine simultaneously (more efficient than separate calls)
 * @x: angle in radians
 * @sinp: pointer to store sine result
 * @cosp: pointer to store cosine result
 */
static inline void b3d_sincosf(float x, float *sinp, float *cosp)
{
    b3d_math_fixed_t angle = B3D_MATH_FLOAT_TO_FP(x);
    b3d_math_fixed_t sin_fp, cos_fp;
    b3d_math_fp_sincos(angle, &sin_fp, &cos_fp);
    *sinp = B3D_MATH_FP_TO_FLOAT(sin_fp);
    *cosp = B3D_MATH_FP_TO_FLOAT(cos_fp);
}

#endif /* B3D_FLOAT_POINT */

#endif /* B3D_MATH_UTILS_DEFINED */

#endif /* B3D_PUBLIC_MATH_H */
