#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

#define FP_SHIFT 14
typedef int real_t;

#define FP_F (1 << FP_SHIFT)

/* Convert n to fixed-point. */
#define FP_CONST(n) ((n) * FP_F)

/* Convert fixed-point to integer (rounding toward zero). */
#define FP_INT_ZERO(x) ((x) / FP_F)

/* Convert fixed-point to integer (rounding to nearest). */
#define FP_INT_ROUND(x)                                                        \
  (((x) >= 0) ? (((x) + (FP_F / 2)) / FP_F) : (((x) - (FP_F / 2)) / FP_F))

/* Add two fixed-point numbers. */
#define FP_ADD(x, y) ((x) + (y))

/* Subtract y from x. */
#define FP_SUB(x, y) ((x) - (y))

/* Add fixed-point x and integer n. */
#define FP_ADD_MIX(x, n) ((x) + ((n) * FP_F))

/* Subtract integer n from fixed-point x. */
#define FP_SUB_MIX(x, n) ((x) - ((n) * FP_F))

/* Multiply fixed-point x by fixed-point y. */
#define FP_MULT(x, y) ((real_t)(((int64_t)(x)) * (y) / FP_F))

/* Multiply fixed-point x by integer n. */
#define FP_MULT_MIX(x, n) ((x) * (n))

/* Divide fixed-point x by fixed-point y. */
#define FP_DIV(x, y) ((real_t)((((int64_t)(x)) * FP_F) / (y)))

/* Divide fixed-point x by integer n. */
#define FP_DIV_MIX(x, n) ((x) / (n))

#endif /* threads/fixed-point.h */
