#ifndef __SECURE_UTILS_H__
#define __SECURE_UTILS_H__


#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/utils.h"


void authenticate_over_field(int sz, float* reals, IntFp* fields, int party){
    uint64_t unsigned_scaled_real = 0;
    for(int i = 0; i < sz; i++){
        if(party == ALICE){
            int64_t scaled_real = reals[i] * (1 << FXPSCALE);
            unsigned_scaled_real = (scaled_real > 0 ? scaled_real : PR + scaled_real);
        }
        
        fields[i] = IntFp(unsigned_scaled_real, ALICE);
    }
}


uint64_t cleartext_inner_product_over_field(int sz, IntFp* x, IntFp* y){
    uint64_t res = 0, tmp;
    for(int i = 0; i < sz; i++){
        tmp = mult_mod(HIGH64(x[i].value), HIGH64(y[i].value));
        res = add_mod(res, tmp);
    }
    return res;
}


IntFp inner_product_bundle(int sz, IntFp* x, IntFp* y, int party){
    /*
        sz: number of actual elements in vectors x and y
        x, y: original vectors consisting of sz authenticated values
    */

    IntFp* vec = new IntFp[2*sz + 2];
    for(int i = 0; i < sz; i++){
        vec[i] = x[i];
        vec[i+sz+1] = y[i];
    }

    uint64_t res = 0;
    if(party == ALICE){
        res = cleartext_inner_product_over_field(sz, x, y);
    } 
    
    vec[sz] = FIELD_MINUS_ONE;
    vec[2*sz + 1] = IntFp(res, ALICE);

    fp_zkp_inner_prdt<BoolIO<NetIO>>(vec, vec + sz + 1, 0, sz + 1);


    IntFp ip_res = vec[2*sz + 1];
    delete[] vec;

    return ip_res;
}


std::pair<IntFp*, IntFp*> relu_bundle(int sz, IntFp* prev_lbs, IntFp* prev_ubs, int party){
    uint64_t* clt_lamus = new uint64_t[2*sz];    // lambdas || mus
    IntFp* lamus = new IntFp[2*sz];

    if(party == ALICE){
        // cleartext coeff computation
        for(int i = 0; i < sz; i++){
            uint64_t u_i = HIGH64(prev_ubs[i].value);
            uint64_t l_i = HIGH64(prev_lbs[i].value);
            uint64_t ul_i = mult_mod(u_i, PR - l_i) >> FXPSCALE;   // -(u_i * l_i) with restored scale
            uint64_t width = add_mod(u_i, PR - l_i);               // u_i - l_i

            clt_lamus[i]      = divide<uint64_t>(u_i, width);      // lambda
            clt_lamus[i + sz] = divide<uint64_t>(ul_i, width);     // mu
        }
    }

    for(int i = 0; i < 2*sz; i++){
        lamus[i] = IntFp(clt_lamus[i], ALICE);
    }

    IntFp* lbpos = new IntFp[sz];
    ZKcmpPositive(party, prev_lbs, ZERO_COMP_CONSTANT, lbpos, sz);

    IntFp* ubpos = new IntFp[sz];
    ZKcmpPositive(party, prev_ubs, ZERO_COMP_CONSTANT, ubpos, sz);

    IntFp* wtilde_pos = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        wtilde_pos[i] = prev_lbs[i] + prev_ubs[i];
    }
    ZKcmpPositive(party, wtilde_pos, ZERO_COMP_CONSTANT, wtilde_pos, sz);

    IntFp* lc_coeffs = new IntFp[2*sz];
    for(int i = 0; i < sz; i++){
        lc_coeffs[i] =   FIELD_ZERO  
                        + lbpos[i] * FIELD_SCALED_ONE 
                        + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * wtilde_pos[i] * FIELD_SCALED_ONE;

        lc_coeffs[i + sz] = FIELD_ZERO;
    }

    IntFp* uc_coeffs = new IntFp[2*sz];
    for(int i = 0; i < sz; i++){
        uc_coeffs[i]        =   FIELD_ZERO  
                                + lbpos[i] * FIELD_SCALED_ONE 
                                + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * lamus[i];

        uc_coeffs[i + sz]   =   FIELD_ZERO 
                                + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * lamus[i + sz];
    }

    return std::make_pair(lc_coeffs, uc_coeffs);
}


std::pair<IntFp*, IntFp*> relu_bundle2(int sz, IntFp* prev_lbs, IntFp* prev_ubs, int party){
    uint64_t* clt_lamus = new uint64_t[2*sz];    // lambdas || mus
    IntFp* lamus = new IntFp[2*sz];

    IntFp* width = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        width[i] = prev_ubs[i] + prev_lbs[i].negate();
    }

    IntFp* prod = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        prod[i] = prev_ubs[i] * prev_lbs[i].negate();
    }
    ZKgeneralTruncAny(party, prod, prod, sz, FXPSCALE);

    if(party == ALICE){
        // cleartext coeff computation
        for(int i = 0; i < sz; i++){
            uint64_t u_i = HIGH64(prev_ubs[i].value);
            uint64_t ul_i = HIGH64(prod[i].value);                   // -(u_i * l_i) with restored scale
            uint64_t width_i = HIGH64(width[i].value);               // u_i - l_i

            clt_lamus[i]      = divide<uint64_t>(u_i, width_i);      // lambda
            clt_lamus[i + sz] = divide<uint64_t>(ul_i, width_i);     // mu
        }
    }

    for(int i = 0; i < 2*sz; i++){
        lamus[i] = IntFp(clt_lamus[i], ALICE);
    }


    for(int i = 0; i < sz; i++){
        inner_product_bundle(1, prev_ubs + i, lamus + i, party);
        inner_product_bundle(1, prod + i, lamus + sz + i, party);
    }
    

    IntFp* lbpos = new IntFp[sz];
    ZKcmpPositive(party, prev_lbs, ZERO_COMP_CONSTANT, lbpos, sz);

    IntFp* ubpos = new IntFp[sz];
    ZKcmpPositive(party, prev_ubs, ZERO_COMP_CONSTANT, ubpos, sz);

    IntFp* wtilde_pos = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        wtilde_pos[i] = prev_lbs[i] + prev_ubs[i];
    }
    ZKcmpPositive(party, wtilde_pos, ZERO_COMP_CONSTANT, wtilde_pos, sz);

    IntFp* lc_coeffs = new IntFp[2*sz];
    for(int i = 0; i < sz; i++){
        lc_coeffs[i] =   FIELD_ZERO  
                        + lbpos[i] * FIELD_SCALED_ONE 
                        + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * wtilde_pos[i] * FIELD_SCALED_ONE;

        lc_coeffs[i + sz] = FIELD_ZERO;
    }

    IntFp* uc_coeffs = new IntFp[2*sz];
    for(int i = 0; i < sz; i++){
        uc_coeffs[i]        =   FIELD_ZERO  
                                + lbpos[i] * FIELD_SCALED_ONE 
                                + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * lamus[i];

        uc_coeffs[i + sz]   =   FIELD_ZERO 
                                + (FIELD_ONE + lbpos[i].negate()) * ubpos[i] * lamus[i + sz];
    }

    return std::make_pair(lc_coeffs, uc_coeffs);
}

#endif