#ifndef __ACTIVATIONS_H_
#define __ACTIVATIONS_H_

#include "emp-tool/emp-tool.h"
#include "emp-zk/ai/utils.h"
#include <iostream>

using namespace emp;
using namespace std;



void relu_layer(int n, float* input, float* output){
    // n neurons in relu layer
    for(int i = 0; i < n; i++){
        output[i] = input[i] > 0 ? input[i] : 0;
    }
}


void relu_layer(int n, Integer* input, Integer* output){
    Integer PUBZERO = Integer(FXPBW, 0, PUBLIC);

    // n neurons in relu layer
    for(int i = 0; i < n; i++){
        Bit c = (input[i] < PUBZERO);
        output[i] = input[i].If(c, PUBZERO);
    }
}


void relu_layer(int n, IntFp* input, IntFp* output){
    Integer PUBZERO = Integer(FXPBW, 0, PUBLIC);

    Integer* input_Integer = new Integer[n];
    for(int i = 0; i < n; i++){
        input_Integer[i] = Integer(FXPBW, input[i].reveal(), PUBLIC);
    }

    // in-place relu
    relu_layer(n, input_Integer, input_Integer);

    // n neurons in relu layer
    for(int i = 0; i < n; i++){
        output[i] = IntFp(input_Integer[i].reveal<uint64_t>(), PUBLIC);
    }
}


void abstract_relu(int n, float* input, float* output){
    
}

#endif