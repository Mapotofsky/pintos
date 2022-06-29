#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define Q 14
#define F (1 << Q)

#define CVT_INT2FP(n) ((int) (n << Q))
#define CVT_FP2INT(x) ((int) (x >> Q))
#define CVT_ROUND_FP2INT(x) ((int) ((x >= 0) ? CVT_FP2INT(x + (1 << (Q - 1))) : CVT_FP2INT(x - (1 << (Q - 1)))))

#define FP_ADD_FP(x, y) (x + y)
#define FP_SUB_FP(x, y) (x - y)

#define FP_ADD_INT(x, n) (x + CVT_INT2FP(n))
#define FP_SUB_INT(x, n) (x - CVT_INT2FP(n))

#define FP_MUL_FP(x, y) CVT_FP2INT(((int64_t) x) * y)
#define FP_DIV_FP(x, y) ((int) (CVT_INT2FP((int64_t) x) / y))

#define FP_MUL_INT(x, n) ((int) (x * n))
#define FP_DIV_INT(x, n) ((int) (x / n))

#endif