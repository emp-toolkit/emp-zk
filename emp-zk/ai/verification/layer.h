#ifndef __LAYER_H__
#define __LAYER_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/ai.h"
#include <iostream>

using namespace emp;
using namespace std;


template <typename T>
class Layer {
    public:
    int input_size;         // n
    int output_size;        // m

    LAYER_TYPE type;
    int layer_num;
    T* input;
    T* output;
    Layer<T>* prev_layer;

    // ABSTRACT INTERPRETATION
    T* lower_bounds;         // length = output_size
    T* upper_bounds;         // length = output_size

    int max_coeffs;         // num coefficients in constraints (not og, backsubstituted constraints)
    T* lower_constraints;   // length = output_size*(prev_layer_neurons+1)
    T* upper_constraints;   // length = output_size*(prev_layer_neurons+1)

    T* backsubstituted_lower_constraints;
    T* backsubstituted_upper_constraints;

    bool is_backsubstituted = false;


    // PROFILING
    double time_for_bs = 0;
    double time_for_fp = 0;
    double time_for_bs_using_act = 0;
    double time_for_bs_using_affine = 0;
    double time_for_bs_in_bounds = 0;


    Layer(int input_size, int output_size, int max_coeffs){
        this->input_size = input_size;
        this->output_size = output_size;
        this->max_coeffs = max_coeffs;
    }

    void compute_lower_constraints();

    void compute_upper_constraints();

    void compute_lower_bounds();

    void compute_upper_bounds();

    virtual void reset();

    virtual void forward(Layer<T>* input_layer, Layer<T>* prev_layer, bool do_inference = true);

    virtual void backsubstitute(Layer<T>* input_layer);

    virtual void describe(bool print_parameters = true, bool print_expressions = false);
};


#endif