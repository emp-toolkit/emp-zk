#ifndef __UTILS_H__
#define __UTILS_H__

// #pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"

#include <fstream>

#define FLOATBW 32
#define FXPBW 61
#define FXPSCALE 24

#define REVERSE(s) reversed(s)

using namespace std;
using namespace emp;

// IntFp FIELD_ZERO(0, PUBLIC);

enum LAYER_TYPE{INPUT, AFFINE, RELU, OUTPUT};
enum TEST_MODE{CLTFLOAT, CLTFXP, SECURE};

std::string get_layer_type(LAYER_TYPE type){
    switch(type){
        case INPUT:
            return "INPUT";
        case AFFINE:
            return "AFFINE";
        case RELU:
            return "RELU";
        case OUTPUT:
            return "OUTPUT";
    }
    return "INVALID_TYPE";
}

std::string reversed(string s){
    std::reverse((s).begin(), (s).end()); 
    return s;
}


template <typename T>
bool greater_eq(T x, T y, bool do_greater);

template <>
bool greater_eq<float>(float x, float y, bool do_greater){
    bool res;
    if (!do_greater){
        res = (x >= y);
    } else {
        res = (x > y);
    }
    return res;
}

template <>
bool greater_eq<IntFp>(IntFp x, IntFp y, bool do_greater){
    bool res;
    if (!do_greater){
        res = (Integer(FXPBW, x.reveal()).geq(Integer(FXPBW, y.reveal()))).reveal<bool>();
    } else {
        res = (Integer(FXPBW, x.reveal()) > (Integer(FXPBW, y.reveal()))).reveal<bool>();
    }
    return res;
}



template <typename T>
bool greater_eq_zero(T x,  bool do_greater);

template <>
bool greater_eq_zero<float>(float x, bool do_greater){
    bool res;
    if (!do_greater){
        res = (x >= 0);
    } else {
        res = (x > 0);
    }
    return res;
}

template <>
bool greater_eq_zero<IntFp>(IntFp x, bool do_greater){
    IntFp ZERO(0, PUBLIC);
    return greater_eq<IntFp>(x, ZERO, do_greater);
}





template <typename T>
T divide(T x, T y);

template <>
float divide<float>(float x, float y){
    return x / y;
}

template <>
IntFp divide<IntFp>(IntFp x, IntFp y){
    Integer x_Integer(FXPBW, x.reveal() << FXPSCALE, PUBLIC);
    Integer y_Integer(FXPBW, y.reveal(), PUBLIC);
    Integer res = x_Integer / y_Integer;

    return IntFp(res.reveal<uint64_t>(), PUBLIC);
}



template <typename T>
T subtract(T x, T y);

template <>
float subtract<float>(float x, float y){
    return x - y;
}

template <>
IntFp subtract<IntFp>(IntFp x, IntFp y){
    IntFp res(x + y.negate());
    return res;
}




template <typename T>
T constant(float x);

template<>
float constant<float>(float x){
    return x;
}

template<>
IntFp constant<IntFp>(float x){
    IntFp c(0, PUBLIC);
    if(x >= 0){
        c = IntFp(x*(1 << FXPSCALE), PUBLIC);
    } else {
        c = IntFp(PR + x*(1 << FXPSCALE), PUBLIC);
    }
    return c;
}



float format_EMP_Integer(Integer a_Integer, int scale_depth = 1){
    float a_float = (
        a_Integer.geq(Integer(FXPBW, 0, PUBLIC)).reveal<bool>() ? 
        static_cast<int64_t>(a_Integer.reveal<uint64_t>()) : 
        static_cast<int64_t>(a_Integer.reveal<uint64_t>()) - static_cast<int64_t>(PR)
    )*1.0;

    while(scale_depth--){
        a_float = a_float / (1 << FXPSCALE);
    }

    return a_float;
}

float format_EMP_IntFp(IntFp a_IntFp, int scale_depth){
    Integer a_Integer(FXPBW, a_IntFp.reveal(), PUBLIC);

    return format_EMP_Integer(a_Integer, scale_depth);
}


// representation conversions
int64_t* convert_reals_to_fixed_point_rep(int sz, float* reals){
    int64_t* fixed_point_integers = new int64_t[sz];
    for(int i = 0; i < sz; i++){
        fixed_point_integers[i] = reals[i]*(1 << FXPSCALE); 
    }
    return fixed_point_integers;
}

Integer* convert_fixed_point_to_emp_Integers(int sz, int64_t* fixed_point_integers){
    Integer* emp_Integers = new Integer[sz];
    for(int i = 0; i < sz; i++){
        uint64_t ring_rep = fixed_point_integers[i] > 0 ? fixed_point_integers[i] : PR + fixed_point_integers[i];
        emp_Integers[i] = new Integer(FXPBW, ring_rep);
    }
    return emp_Integers;
}

IntFp* convert_fixed_point_to_field_rep(int sz, int64_t* fixed_point_integers){
    IntFp* field_elements = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        uint64_t ring_rep = fixed_point_integers[i] > 0 ? fixed_point_integers[i] : PR + fixed_point_integers[i];
        field_elements[i] = ring_rep;
    }
    return field_elements;
}

IntFp* convert_reals_to_field_rep(int sz, float* reals){
    IntFp* field_elements = convert_fixed_point_to_field_rep(
        sz, convert_reals_to_fixed_point_rep(sz, reals)
    );
    return field_elements;
}


float* convert_field_rep_to_reals(int sz, IntFp* field_elements){
    float* reals = new float[sz];
    for(int i = 0; i < sz; i++){
        uint64_t el_fxp = field_elements[i].reveal();
        int64_t signed_fxp = el_fxp;
        if (el_fxp > (PR-1)/2){
            // negative
            signed_fxp = static_cast<int>(el_fxp) - static_cast<int>(PR);
        }
        reals[i] = signed_fxp;
    }
    return reals;
}


Integer* convert_field_rep_to_emp_Integer(int sz, IntFp* field_elements){
    Integer* emp_Integers = new Integer[sz];
    for(int i = 0; i < sz; i++){
        emp_Integers[i] = Integer(FXPBW, field_elements[i].reveal(), PUBLIC);
    }
    return emp_Integers;
}


// file i/o
void read_next_elements(size_t n, float* buffer, size_t offset, const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + string(filepath));
    }

    if (buffer == NULL){
        buffer = new float[n];
    }

    size_t pos = 0;
    while (file >> buffer[pos]) {
        if(offset > 0){
            offset--;
            continue;
        }

        pos++;

        if(pos == n){
            break;
        }
    }
}

#endif