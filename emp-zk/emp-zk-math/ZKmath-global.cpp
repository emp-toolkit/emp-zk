#include "ZKmath-global.h"

// LUTRange
LUTRangeIntFp *LUTRange[NUM_RANGE];

// LUTmsnzb
LUTTwoValueIntFp *LUTmsnzb2value;

// LUTdiv
LUTTwoValueIntFp *LUTdiv;

// LUTextend
LUTIntFp *LUTextend;

// LUTexp
LUTIntFp *LUTexp[EXP_LUT_NUM];

// LUTsqrt 
LUTIntFp *LUTsqrt;
LUTIntFp *LUTsqrtExtend;

// LUTcmp
uint64_t FINIAL_CMP_LUT_NUM = 0;
uint64_t CMP_LAST_DIGIT_BITLEN = 0;
LUTIntFp *LUTvrfyCmpLx[CMP_LUT_NUM];
LUTIntFp *LUTvrfyCmpLy;
LUTIntFp *LUTvrfyCmpLx_InP[CMP_LUT_NUM];
//
LUTTwoValueIntFp *LUTCmpLx[CMP_LUT_NUM];

void startComputation(int party)
{
    // LUTRange
    for (int i = 2; i < NUM_RANGE; i++){  
        LUTRange[i] = new LUTRangeIntFp(party);
        LUTRange[i]->LUTRangeinit(1ULL << i);
    }

    uint64_t lutSize = ceil(log2(PR)) - 1;
    vector<uint64_t> coff1;
    vector<uint64_t> coff2;
    LUTmsnzb2value = new LUTTwoValueIntFp(party);
    for (int i = 0; i < lutSize - 1; i++){
        coff1.push_back(1ULL << i);
        coff2.push_back((1ULL << (i + 1)) - 1);
    }
    coff1.push_back(1ULL << (lutSize - 1));
    coff2.push_back((PR - 1)/2);
    LUTmsnzb2value->LUTTwoValueinit(coff1, coff2);
    coff1.resize(0);
    coff2.resize(0);

    // LUTdiv
    LUTdiv = new LUTTwoValueIntFp(party);
    int32_t k = 1ULL << DIV_M;
    double p = 0;
    double z = 0;
    double A0 = 0;
    int32_t scale_a = SCALE + DIV_N - 1;
    uint64_t val = 0;
    double A1 = 0;
    for (int i = 0; i < k; i++){
        // a \in (1/2, 1)
        p = 1 + (double(i) / double(k));
        z = p * (p + (1.0 / double(k)));
        A0 = ((1.0 / double(k * 2)) + sqrt(z)) / z;
        val = A0 * (1ULL << scale_a);
        coff1.push_back(val);

        // b \in (1/4, 1)
        A1 = 1.0 / z;
        val = A1 * (1ULL << SCALE);
        coff2.push_back(val);
    }
    LUTdiv->LUTTwoValueinit(coff1, coff2);
    coff1.resize(0);
    coff2.resize(0);

    // LUTextend   index \in [0, ceil(logP) - 1]
    vector<uint64_t> data;
    LUTextend = new LUTIntFp(party);
    lutSize = ceil(log2(PR));
    for (int i = 0; i < lutSize; i++){
        data.push_back(1ULL << i); 
    }
    LUTextend->LUTinit(data);
    data.resize(0);

    // LUTexp
    lutSize = 1ULL << EXP_DIGIT_LEN;
    uint64_t last_lutsize = 1ULL << (EXP_N - EXP_DIGIT_LEN * (EXP_LUT_NUM - 1));
    k = 1ULL << SCALE;
    double j_real = 0;
    double value_real = 0;
    for (int i = 0; i < EXP_LUT_NUM - 1; i++){  
        LUTexp[i] = new LUTIntFp(party);
        for (int j = 0; j < lutSize; j++){
            j_real = double(j) / double(k);
            value_real = exp(-1 * (j_real * (1ULL << EXP_DIGIT_LEN * i)));
            data.push_back((uint64_t)(value_real * k));
        }
        LUTexp[i]->LUTinit(data);
        data.resize(0);
    }
    LUTexp[EXP_LUT_NUM - 1] = new LUTIntFp(party);
    for (int j = 0; j < last_lutsize; j++){
        j_real = double(j) / double(k);
        value_real = exp(-1 * (j_real * (1ULL << EXP_DIGIT_LEN * (EXP_LUT_NUM - 1))));
        data.push_back((uint64_t)(value_real * k));
    }
    LUTexp[EXP_LUT_NUM - 1]->LUTinit(data);
    data.resize(0);

    // LUTsqrt 
    LUTsqrtExtend = new LUTIntFp(party);
    uint64_t value = 0;
    for (int i = 0; i < SQRT_N; i++){   
        value = floor((double)(SCALE + 1 - (int)i)/2.0) + (double)(SQRT_N - SCALE)/2.0;
        data.push_back(1ULL << value); 
    }
    LUTsqrtExtend->LUTinit(data);
    data.resize(0);

    LUTsqrt = new LUTIntFp(party);
    k = 1ULL << SQRT_M;
    lutSize = (1ULL << (SQRT_M + 1));
    for (int i = 0; i < lutSize; i++){
        uint64_t k0 = i & 1ULL;
        uint64_t z1 = (i >> 1ULL) & ((1ULL << SQRT_M) - 1);
        p = 1 + (double(z1) / double(k));
        value_real = 1/sqrt((double)((k0 + 1) * p));      
        data.push_back((uint64_t)(value_real * (1ULL << SCALE)));
    }
    LUTsqrt->LUTinit(data);
    data.resize(0);

    // LUTcmp  -  verify  -  constant = (p+1)/2
    FINIAL_CMP_LUT_NUM = CMP_LUT_NUM;
    CMP_LAST_DIGIT_BITLEN = BIT_LENGTH - CMP_DIGIT_LEN * (FINIAL_CMP_LUT_NUM - 1);
    if (CMP_LAST_DIGIT_BITLEN == 1){
        FINIAL_CMP_LUT_NUM = CMP_LUT_NUM - 1;
        CMP_LAST_DIGIT_BITLEN = CMP_DIGIT_LEN + 1;
    }
    lutSize = 1ULL << CMP_DIGIT_LEN;
    last_lutsize = 1ULL << CMP_LAST_DIGIT_BITLEN;

    // decomposite constant = (p+1)/2
    uint64_t c = (PR + 1)/2;                                      
    uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
    uint64_t c_digit_mask = (1ULL << CMP_DIGIT_LEN) - 1;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
        c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & c_digit_mask;
    }
    c_digit_mask = (1ULL << CMP_LAST_DIGIT_BITLEN) - 1;
    c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & c_digit_mask;
    // construct LUT - LUTvrfyCmpLx
    uint64_t tmp = 0;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
        LUTvrfyCmpLx[i] = new LUTIntFp(party);
        for (int j = 0; j < lutSize; j++){
            uint64_t res1 = 0;
            uint64_t res2 = 0;
            if (j == c_digit[i]){
                res1 = 1;
            }
            if (j < c_digit[i]){
                res2 = 1;
            }
            tmp = (1ULL << (FINIAL_CMP_LUT_NUM + i)) * res1 + (1ULL << i) * res2;
            data.push_back(tmp);
        }
        LUTvrfyCmpLx[i]->LUTinit(data);
        data.resize(0);
    }
    LUTvrfyCmpLx[FINIAL_CMP_LUT_NUM - 1] = new LUTIntFp(party);
    for (int j = 0; j < last_lutsize; j++){
        uint64_t res1 = 0;
        uint64_t res2 = 0;
        if (j == c_digit[FINIAL_CMP_LUT_NUM - 1]){
            res1 = 1;
        }
        if (j < c_digit[FINIAL_CMP_LUT_NUM - 1]){
            res2 = 1;
        }
        tmp = (1ULL << (FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1))) * res1 + (1ULL << (FINIAL_CMP_LUT_NUM - 1)) * res2;
        data.push_back(tmp);
    }
    LUTvrfyCmpLx[FINIAL_CMP_LUT_NUM - 1]->LUTinit(data);
    data.resize(0);
    // construct LUT - LUTvrfyCmpLy
    LUTvrfyCmpLy = new LUTIntFp(party);
    vector<uint64_t> key;
    uint64_t a1 = 0;
    uint64_t a2 = 0;
    // uint64_t count = 0;
    for (uint64_t eq = 0; eq < (1ULL << FINIAL_CMP_LUT_NUM); eq++){
        for (uint64_t lt = 0; lt < (1ULL << FINIAL_CMP_LUT_NUM); lt++){
            if ((eq & lt) > 0){
                continue;
            }
            // count++;
            // compute key
            a1 = 1ULL << (FINIAL_CMP_LUT_NUM + 0);
            a2 = 1ULL << 0;
            tmp = a1 * ((eq >> 0) & (1ULL)) +  a2 * ((lt >> 0) & (1ULL));
            for (uint64_t i = 1; i < FINIAL_CMP_LUT_NUM; i++){
                a1 = 1ULL << (FINIAL_CMP_LUT_NUM + i);
                a2 = 1ULL << i;
                tmp = tmp + a1 * ((eq >> i) & (1ULL)) + a2 * ((lt >> i) & (1ULL));
            }
            key.push_back(tmp);
            // compute value
            tmp = (lt >> 0) & (1ULL);
            for (uint64_t i = 1; i < FINIAL_CMP_LUT_NUM; i++){
                if (tmp > 1){exit(0);}
                tmp = ((lt >> i) & (1ULL)) + ((eq >> i) & (1ULL)) * tmp;
            }
            data.push_back(tmp);
        }
    }
    // cout << "count = " << count << endl;
    LUTvrfyCmpLy->LUTDisInit(key, data);
    key.resize(0);
    data.resize(0);

    // decomposite constant = p
    uint64_t p_cmp = PR;       
    uint64_t *p_digit = new uint64_t[FINIAL_CMP_LUT_NUM];                             
    uint64_t p_digit_mask = (1ULL << CMP_DIGIT_LEN) - 1;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
        p_digit[i] = (p_cmp >> (i * CMP_DIGIT_LEN)) & p_digit_mask;
    }
    p_digit_mask = (1ULL << CMP_LAST_DIGIT_BITLEN) - 1;
    p_digit[FINIAL_CMP_LUT_NUM - 1] = (p_cmp >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & p_digit_mask;
    // construct LUT - LUTvrfyCmpLx_InP
    tmp = 0;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
        LUTvrfyCmpLx_InP[i] = new LUTIntFp(party);
        for (int j = 0; j < lutSize; j++){
            uint64_t res1 = 0;
            uint64_t res2 = 0;
            if (j == p_digit[i]){
                res1 = 1;
            }
            if (j < p_digit[i]){
                res2 = 1;
            }
            tmp = (1ULL << (FINIAL_CMP_LUT_NUM + i)) * res1 + (1ULL << i) * res2;
            data.push_back(tmp);
        }
        LUTvrfyCmpLx_InP[i]->LUTinit(data);
        data.resize(0);
    }
    LUTvrfyCmpLx_InP[FINIAL_CMP_LUT_NUM - 1] = new LUTIntFp(party);
    for (int j = 0; j < last_lutsize; j++){
        uint64_t res1 = 0;
        uint64_t res2 = 0;
        if (j == p_digit[FINIAL_CMP_LUT_NUM - 1]){
            res1 = 1;
        }
        if (j < p_digit[FINIAL_CMP_LUT_NUM - 1]){
            res2 = 1;
        }
        tmp = (1ULL << (FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1))) * res1 + (1ULL << (FINIAL_CMP_LUT_NUM - 1)) * res2;
        data.push_back(tmp);
    }
    LUTvrfyCmpLx_InP[FINIAL_CMP_LUT_NUM - 1]->LUTinit(data);
    data.resize(0);

    // LUTcmp  -  computation 
    // // construct LUT - LUTCmpLx
    tmp = 0;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
        LUTCmpLx[i] = new LUTTwoValueIntFp(party);
        for (int j = 0; j < lutSize; j++){
            uint64_t res1 = 0;
            uint64_t res2 = 0;
            if (j == c_digit[i]){
                res1 = 1;
            }
            if (j < c_digit[i]){
                res2 = 1;
            }
            tmp = (1ULL << (FINIAL_CMP_LUT_NUM + i)) * res1 + (1ULL << i) * res2;
            coff1.push_back(tmp);

            res1 = 0;
            res2 = 0;
            if (j == p_digit[i]){
                res1 = 1;
            }
            if (j < p_digit[i]){
                res2 = 1;
            }
            tmp = (1ULL << (FINIAL_CMP_LUT_NUM + i)) * res1 + (1ULL << i) * res2;
            coff2.push_back(tmp);
        }
        LUTCmpLx[i]->LUTTwoValueinit(coff1, coff2);
        coff1.resize(0);
        coff2.resize(0);
    }
    LUTCmpLx[FINIAL_CMP_LUT_NUM - 1] = new LUTTwoValueIntFp(party);
    for (int j = 0; j < last_lutsize; j++){
        uint64_t res1 = 0;
        uint64_t res2 = 0;
        if (j == c_digit[FINIAL_CMP_LUT_NUM - 1]){
            res1 = 1;
        }
        if (j < c_digit[FINIAL_CMP_LUT_NUM - 1]){
            res2 = 1;
        }
        tmp = (1ULL << (FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1))) * res1 + (1ULL << (FINIAL_CMP_LUT_NUM - 1)) * res2;
        coff1.push_back(tmp);

        res1 = 0;
        res2 = 0;
        if (j == p_digit[FINIAL_CMP_LUT_NUM - 1]){
            res1 = 1;
        }
        if (j < p_digit[FINIAL_CMP_LUT_NUM - 1]){
            res2 = 1;
        }
        tmp = (1ULL << (FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1))) * res1 + (1ULL << (FINIAL_CMP_LUT_NUM - 1)) * res2;
        coff2.push_back(tmp);
    }
    LUTCmpLx[FINIAL_CMP_LUT_NUM - 1]->LUTTwoValueinit(coff1, coff2);
    coff1.resize(0);
    coff2.resize(0);

    delete[] c_digit;
    delete[] p_digit;
}

void endComputation(int party)
{
    for (int i = 2; i < NUM_RANGE; i++){  
        delete LUTRange[i];
    }
    delete LUTmsnzb2value;
    delete LUTdiv;
    delete LUTextend;
    for (int i = 0; i < EXP_LUT_NUM; i++){  
        delete LUTexp[i];
    }
    delete LUTsqrtExtend;
    delete LUTsqrt;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM; i++){
        delete LUTvrfyCmpLx[i];
    }
    delete LUTvrfyCmpLy;
    for (int i = 0; i < FINIAL_CMP_LUT_NUM; i++){
        delete LUTvrfyCmpLx_InP[i];
    }
}

uint64_t Real2Field(double x, int scale = SCALE){
    int64_t x_scale = floor(x * (1ULL << scale));
    // cout << "x_scale = " << x_scale << endl;
    uint64_t y = x_scale < 0 ? PR + x_scale : x_scale; 
    return y;
}

double Field2Real(uint64_t x, int scale = SCALE){
    uint64_t c = x > (PR - 1)/2 ? 1 : 0;
    int64_t x_scale = x - c * PR;
    double y = (double)x_scale / (double)(1ULL << scale);
    return y;
}

uint64_t computeULPErr(int64_t calc_fixed, int64_t actual_fixed)
{
  uint64_t ulp_err = (calc_fixed - actual_fixed) > 0
                         ? (calc_fixed - actual_fixed)
                         : (actual_fixed - calc_fixed);
  return ulp_err;
}
