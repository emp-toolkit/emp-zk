#ifndef __AFFINE_H__
#define __AFFINE_H

#include "emp-tool/emp-tool.h"
#include "emp-zk/ai/utils.h"
#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
T inner_product_emp(int n, T* x, T* y);

template<>
float inner_product_emp(int n, float* x, float* y){
    float sum = 0;
    for(int i = 0; i < n; i++){
        sum += x[i]*y[i];
    }
    return sum;
}

template<>
int64_t inner_product_emp(int n, int64_t* x, int64_t* y){
    int64_t sum = 0;
    for(int i = 0; i < n; i++){
        sum += x[i]*y[i];
    }
    return sum;
}

template<>
Integer inner_product_emp(int n, Integer* x, Integer* y){
    Integer sum(FXPBW, 0, PUBLIC);
    sum = sum * Integer(FXPBW, 1 << FXPSCALE, PUBLIC);
    for(int i = 0; i < n; i++){
        sum = sum + x[i]*y[i];
    }
    return sum;
}

template<>
IntFp inner_product_emp(int n, IntFp* x, IntFp* y){
    IntFp sum(0, PUBLIC);
    for(int i = 0; i < n; i++){
        // cout << format_EMP_IntFp(x[i], 1) << " ";
        // cout << format_EMP_IntFp(y[i], 1) << ", ";
        sum = sum + x[i]*y[i];
    }
    return sum;
}


void affine_layer(int m, int n, float* params, float* input, float* output){
    // params = Z^{m x n+1} [A_i | b_i]
    // input = Z^{n+1}

    for(int i = 0; i < m; i++){
        output[i] = inner_product_emp(n+1, params + i*(n+1), input);
    }
}

void affine_layer(int m, int n, int64_t* params, int64_t* input, int64_t* output){
    // params = Z^{m x n+1} [A_i | b_i]
    // input = Z^{n+1}

    for(int i = 0; i < m; i++){
        output[i] = inner_product_emp(n+1, params + i*(n+1), input);
    }
}

void affine_layer(int m, int n, Integer* params, Integer* input, Integer* output){
    // params = Z^{m x n+1} [A_i | b_i]
    // input = Z^{n+1}

    for(int i = 0; i < m; i++){
        output[i] = inner_product_emp(n+1, params + i*(n+1), input);
    }
}

void affine_layer(int m, int n, IntFp* params, IntFp* input, IntFp* output){
    // params = Z^{m x n+1} [A_i | b_i]
    // input = Z^{n+1}
    
    for(int i = 0; i < m; i++){
        output[i] = inner_product_emp(n+1, params + i*(n+1), input);
    }
}





#endif