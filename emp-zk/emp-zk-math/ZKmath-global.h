#include "emp-zk/emp-zk-math/LUT-range.h"
#include "emp-zk/emp-zk-math/LUT.h"
#include "emp-zk/emp-zk-math/LUT-twoValue.h"

#define BIT_LENGTH 61
#define SCALE 12

// LUTRange
#define NUM_RANGE 13
extern LUTRangeIntFp *LUTRange[NUM_RANGE];  // 0~12

// LUTdiv
#define DIV_M 5     //(SCALE - 2)/2
#define DIV_N 20
// #define DIV_N 13
extern LUTTwoValueIntFp *LUTdiv;

// LUTextend
extern LUTIntFp *LUTextend;

// LUTexp
#define EXP_N 16
#define EXP_DIGIT_LEN 12
#define EXP_LUT_NUM 2        // EXP_LUT_NUM = ceil(EXP_N/EXP_DIGIT_LEN)
extern LUTIntFp *LUTexp[EXP_LUT_NUM]; 

// sqrt
#define SQRT_M 6    // SCALE/2
#define SQRT_N 20
extern LUTIntFp *LUTsqrt;
extern LUTIntFp *LUTsqrtExtend;

// LUTmsnzb
extern LUTTwoValueIntFp *LUTmsnzb2value;

// LUTcmp - verify
#define CMP_DIGIT_LEN 12
#define CMP_LUT_NUM 6        // CMP_LUT_NUM = ceil(BIT_LENGTH/CMP_DIGIT_LEN)
extern uint64_t FINIAL_CMP_LUT_NUM;
extern uint64_t CMP_LAST_DIGIT_BITLEN;
extern LUTIntFp *LUTvrfyCmpLx[CMP_LUT_NUM];
extern LUTIntFp *LUTvrfyCmpLy;
extern LUTIntFp *LUTvrfyCmpLx_InP[CMP_LUT_NUM];
// LUTcmp - computation
extern LUTTwoValueIntFp *LUTCmpLx[CMP_LUT_NUM];

void startComputation(int party);
void endComputation(int party);

uint64_t Real2Field(double x, int scale);
double Field2Real(uint64_t x, int scale);
uint64_t computeULPErr(int64_t calc_fixed, int64_t actual_fixed);