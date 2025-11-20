#ifndef __BOUND_UTILS_H__
#define __BOUND_UTILS_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"

#include <fstream>

using namespace std;
using namespace emp;


/* ===================== LOWER CONSTRAINTS ===================== */

template <typename T>
void update_lower_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
    assert(current_layer->max_coeffs == prev_layer->output_size + 1 && "Current layer's no. of coeffs should match (prev. layer's num neurons + 1).");
    auto start = clock_start();

    // update lower bounds
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
        T* prev_layer_bounds_to_mult = new T[current_layer->max_coeffs];     

        for(int j = 0; j < prev_layer->output_size; j++){
            if(greater_eq_zero<T>((T) current_lower_constraints[j], false)){
                prev_layer_bounds_to_mult[j] = prev_layer->lower_bounds[j];
            } else {
                prev_layer_bounds_to_mult[j] = prev_layer->upper_bounds[j];
            }
        }
        prev_layer_bounds_to_mult[current_layer->max_coeffs-1] = constant<T>(1);    // for constant term 

        current_layer->lower_bounds[i] = inner_product_emp(current_layer->max_coeffs, current_lower_constraints, prev_layer_bounds_to_mult);
    }

    if constexpr (std::is_same<IntFp, T>::value){
        normalize(current_layer->output_size, current_layer->lower_bounds, current_layer->lower_bounds);
    }
    double tt = time_from(start);
    current_layer->time_for_bs_in_bounds += tt;

    start = clock_start();
    // update lower constraints
    if (prev_layer->type == INPUT){
        ;
    } else if(prev_layer->type == RELU){
        update_lower_constraints_with_activation(current_layer, prev_layer);
    } else {
        update_lower_constraints_with_affine(current_layer, prev_layer);
    }
    tt = time_from(start);

    if(prev_layer->type == RELU){
        current_layer->time_for_bs_using_act += tt;
    } else {
        current_layer->time_for_bs_using_affine += tt;
    }
}

template <typename T>
void update_lower_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * prev_layer->max_coeffs];

    double time_for_comp = 0;
    double time_for_ip = 0;
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < prev_layer->max_coeffs; k++){        // including constant term
    
            auto start = clock_start();
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_lower_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->lower_constraints[j*prev_layer->max_coeffs + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->upper_constraints[j*prev_layer->max_coeffs + k];
                }    
            }
            double tt = time_from(start);
            time_for_comp += tt;

            start = clock_start();
            new_backsubstituted_lower_constraints[i*prev_layer->max_coeffs + k] = inner_product_emp(prev_layer->output_size, current_lower_constraints, prev_coeffs_to_mult);
            tt = time_from(start);
            time_for_ip += tt;
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(prev_layer->max_coeffs, new_backsubstituted_lower_constraints + i*prev_layer->max_coeffs, new_backsubstituted_lower_constraints + i*prev_layer->max_coeffs);
        }

        // adding constant term to constant product
        new_backsubstituted_lower_constraints[(i + 1)*prev_layer->max_coeffs - 1] = new_backsubstituted_lower_constraints[(i + 1)*prev_layer->max_coeffs - 1] 
                                                                                    + current_lower_constraints[current_layer->max_coeffs - 1];
    }

    // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
    // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

    current_layer->max_coeffs = prev_layer->max_coeffs; // check

    delete[] current_layer->backsubstituted_lower_constraints;
    current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;
}

template <typename T>
void update_lower_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * (prev_layer->input_size + 1)];

    for(int i = 0; i < current_layer->output_size; i++){

        T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
        new_backsubstituted_lower_constraints[(i+1)*(prev_layer->input_size+1) - 1] = current_lower_constraints[current_layer->max_coeffs-1]*constant<T>(1);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size + 1];

        int constant_term_pos = (i+1)*(prev_layer->input_size+1) - 1;
        for(int j = 0; j < prev_layer->output_size; j++){
            if(greater_eq_zero<T>(current_lower_constraints[j], false)){
                new_backsubstituted_lower_constraints[i*(prev_layer->input_size + 1) + j] = current_lower_constraints[j] * prev_layer->lower_constraints[j*2 + 0];
                new_backsubstituted_lower_constraints[constant_term_pos] = new_backsubstituted_lower_constraints[constant_term_pos] + current_lower_constraints[j] * prev_layer->lower_constraints[j*2 + 1];
            } else {
                new_backsubstituted_lower_constraints[i*(prev_layer->input_size + 1) + j] = current_lower_constraints[j] * prev_layer->upper_constraints[j*2 + 0];
                new_backsubstituted_lower_constraints[constant_term_pos] = new_backsubstituted_lower_constraints[constant_term_pos] +  current_lower_constraints[j] * prev_layer->upper_constraints[j*2 + 1];
            }    
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(current_layer->max_coeffs, new_backsubstituted_lower_constraints + i*current_layer->max_coeffs, new_backsubstituted_lower_constraints + i*current_layer->max_coeffs);
        }
    }

    current_layer->max_coeffs = prev_layer->input_size + 1; // check
    
    delete[] current_layer->backsubstituted_lower_constraints;
    current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;
}




/* ===================== UPPER CONSTRAINTS ===================== */
template <typename T>
void update_upper_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
    assert(current_layer->max_coeffs == prev_layer->output_size + 1 && "Current layer's no. of coeffs should match (prev. layer's num neurons + 1).");

    // update upper bounds
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
        T* prev_layer_bounds_to_mult = new T[current_layer->max_coeffs];     

        for(int j = 0; j < prev_layer->output_size; j++){
            if(greater_eq_zero<T>(current_upper_constraints[j], false)){
                prev_layer_bounds_to_mult[j] = prev_layer->upper_bounds[j];
            } else {
                prev_layer_bounds_to_mult[j] = prev_layer->lower_bounds[j];
            }
        }
        prev_layer_bounds_to_mult[current_layer->max_coeffs-1] = constant<T>(1);    // for constant term 

        current_layer->upper_bounds[i] = inner_product_emp(current_layer->max_coeffs, current_upper_constraints, prev_layer_bounds_to_mult);
    }

    if constexpr (std::is_same<IntFp, T>::value){
        normalize(current_layer->output_size, current_layer->upper_bounds, current_layer->upper_bounds);
    }


    // update upper constraints
    if (prev_layer->type == INPUT){
        ;
    } else if(prev_layer->type == RELU){
        update_upper_constraints_with_activation(current_layer, prev_layer);
    } else {
        update_upper_constraints_with_affine(current_layer, prev_layer);
    }
}

template <typename T>
void update_upper_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * prev_layer->max_coeffs];

    for(int i = 0; i < current_layer->output_size; i++){
        T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < prev_layer->max_coeffs; k++){        // including constant term
    
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_upper_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->upper_constraints[j*prev_layer->max_coeffs + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->lower_constraints[j*prev_layer->max_coeffs + k];
                }    
            }

            new_backsubstituted_upper_constraints[i*prev_layer->max_coeffs + k] = inner_product_emp(prev_layer->output_size, current_upper_constraints, prev_coeffs_to_mult);
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(prev_layer->max_coeffs, new_backsubstituted_upper_constraints + i*prev_layer->max_coeffs, new_backsubstituted_upper_constraints + i*prev_layer->max_coeffs);
        }

        // adding constant term to constant product
        new_backsubstituted_upper_constraints[(i + 1)*prev_layer->max_coeffs - 1] = new_backsubstituted_upper_constraints[(i + 1)*prev_layer->max_coeffs - 1] 
                                                                                    + current_upper_constraints[current_layer->max_coeffs - 1];
    }

    current_layer->max_coeffs = prev_layer->max_coeffs; // check

    delete[] current_layer->backsubstituted_upper_constraints;
    current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;
}

template <typename T>
void update_upper_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * (prev_layer->input_size + 1)];


    for(int i = 0; i < current_layer->output_size; i++){
        T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
        new_backsubstituted_upper_constraints[(i+1)*(prev_layer->input_size+1) - 1] = current_upper_constraints[current_layer->max_coeffs-1]*constant<T>(1);
            
        int constant_term_pos = (i+1)*(prev_layer->input_size+1) - 1;
        for(int j = 0; j < prev_layer->output_size; j++){
            if(greater_eq_zero<T>(current_upper_constraints[j], false)){
                new_backsubstituted_upper_constraints[i*(prev_layer->input_size + 1) + j] = current_upper_constraints[j] * prev_layer->upper_constraints[j*2 + 0];
                new_backsubstituted_upper_constraints[constant_term_pos] = new_backsubstituted_upper_constraints[constant_term_pos] + current_upper_constraints[j] * prev_layer->upper_constraints[j*2 + 1];
            } else {
                new_backsubstituted_upper_constraints[i*(prev_layer->input_size + 1) + j] = current_upper_constraints[j] * prev_layer->lower_constraints[j*2 + 0];
                new_backsubstituted_upper_constraints[constant_term_pos] = new_backsubstituted_upper_constraints[constant_term_pos] +  current_upper_constraints[j] * prev_layer->lower_constraints[j*2 + 1];
            }    
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(current_layer->max_coeffs, new_backsubstituted_upper_constraints + i*current_layer->max_coeffs, new_backsubstituted_upper_constraints + i*current_layer->max_coeffs);
        }
    }

    current_layer->max_coeffs = prev_layer->input_size + 1; // check
    
    delete[] current_layer->backsubstituted_upper_constraints;
    current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;
}


#endif