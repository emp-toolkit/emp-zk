#ifndef __NORMALIZE_H__
#define __NORMALIZE_H__

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/utils.h"
#include <iostream>

using namespace emp;
using namespace std;


Integer normalize(Integer x){
    Integer norm_x(FXPBW, 0);
    Bit pos = x.geq(Integer(FXPBW, 0));

    if(pos.reveal<bool>()){
        norm_x = x >> FXPSCALE;
    } else {
        Integer MASK(FXPBW, (1 << FXPSCALE)-1);  // 111...16-bits...11
        MASK = MASK << (FXPBW - FXPSCALE);       // 111...16-bits...110000...(61-16)-bits...00
        
        norm_x = x >> FXPSCALE;
        norm_x = norm_x | MASK;
    }
    return norm_x;
}


void normalize(int n, Integer* input, Integer* output, int current_scale_depth = 2){
    for(int i = 0; i < n; i++){
       output[i] = normalize(input[i]);
    }
}

void normalize(int n, IntFp* input, IntFp* output, int current_scale_depth = 2){
    Integer* input_Integer = new Integer[n];
    for(int i = 0; i < n; i++){
        input_Integer[i] = Integer(FXPBW, input[i].reveal(), PUBLIC);
    }

    normalize(n, input_Integer, input_Integer, current_scale_depth);

    for(int i = 0; i < n; i++){
        output[i] = new IntFp(input_Integer[i].reveal<uint64_t>(), PUBLIC);
    }
}


#endif