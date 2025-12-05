// #include "emp-zk/emp-zk-math/Poly.h"
#include "emp-zk/emp-zk-math/ZKmath-global.h"

# define M_PI           3.14159265358979323846

void ZKPositiveDigDec(int party, IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim);
void ZKPositiveDigDecAny(int party, IntFp *x, IntFp *xhigh, IntFp *xlow, uint64_t highsize, uint64_t lowsize, int dim);

void ZKpositiveTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen);
void ZKgeneralTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen);

// Verify y = 1{x < (p+1)/2}
void ZKcmpRealVrfyPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim);
// Verify y = 1{x < p}
void ZKcmpRealVrfyP(int party, IntFp *x, uint64_t c, IntFp *y, int dim);
// compute y = 1{x < (p+1)/2}
void ZKcmpPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim);

void ZKMax(int party, IntFp *x, IntFp *y, int rows, int cols);

void ZKmsnzb(int party, IntFp *x, IntFp *y, int dim);
void ZKExtend(int party, IntFp *x, IntFp *k, IntFp *y, int dim);
void ZKExtendSqrt(int party, IntFp *x, IntFp *k, IntFp *y, int dim);

void ZKExp(int party, IntFp *x, IntFp *y, int dim);
void ZKDiv(int party, IntFp *x, IntFp *y, int dim);
void ZKrSqrt(int party, IntFp *x, IntFp *y, int dim, int iter);

void ZKSigmoid(int party, IntFp *x, IntFp *y, int dim);
void ZKGeLU(int party, IntFp *x, IntFp *y, int dim);
void ZKSoftmax(int party, IntFp *x, IntFp *y, int rows, int cols);
void ZKLayerNorm(int party, IntFp *x, IntFp *y, IntFp *gamma, IntFp *beta, int rows, int cols);