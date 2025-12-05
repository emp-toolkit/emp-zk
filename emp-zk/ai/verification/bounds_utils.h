#ifndef __BOUND_UTILS_H__
#define __BOUND_UTILS_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include "emp-zk/ai/utils.h"

#include <fstream>

using namespace std;
using namespace emp;


template <typename T>
void update_lower_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
    assert(current_layer->max_coeffs == prev_layer->output_size + 1 && "Current layer's no. of coeffs should match (prev. layer's num neurons + 1).");
    auto start = clock_start();
    double tt;

    if constexpr (std::is_same<IntFp, T>::value && SECURE){
        T* prev_lbs = prev_layer->lower_bounds;
        T* prev_ubs = prev_layer->upper_bounds;

        // update lower bounds
        T* prev_bounds = new T[2*(current_layer->max_coeffs - 1)];       // lb_1 lb_2 ... lb_m   ub_1 ub_2 ... ub_m  
        for(int j = 0; j < current_layer->max_coeffs - 1; j++){
            prev_bounds[j]                                 = prev_lbs[j];
            prev_bounds[j + current_layer->max_coeffs - 1] = prev_ubs[j];
        }

        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        T* copied_lc = new T[2*(current_layer->max_coeffs - 1)];

        for(int i = 0; i < current_layer->output_size; i++){
        
            ZKcmpPositive(current_layer->party, current_layer->backsubstituted_lower_constraints + i*current_layer->max_coeffs, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }

            // first select which bound to multiply based on sign
            for(int j = 0; j < 2*(current_layer->max_coeffs - 1); j++){
                coeff_sign[j] = prev_bounds[j] * coeff_sign[j];
            }

            for(int j = 0; j < current_layer->max_coeffs-1; j++){
                copied_lc[j]                                 = current_layer->backsubstituted_lower_constraints[i*current_layer->max_coeffs + j];
                copied_lc[j + current_layer->max_coeffs - 1] = current_layer->backsubstituted_lower_constraints[i*current_layer->max_coeffs + j];
            }
            
            // inner product
            current_layer->lower_bounds[i] = inner_product_bundle(2*(current_layer->max_coeffs - 1), copied_lc, coeff_sign, current_layer->party);
        }

        delete[] coeff_sign;
        delete[] copied_lc;

        // restore the fixed-point scale
        ZKgeneralTruncAny(current_layer->party, current_layer->lower_bounds, current_layer->lower_bounds, current_layer->output_size, FXPSCALE);

        for(int i = 0; i < current_layer->output_size; i++){
            current_layer->lower_bounds[i] = current_layer->lower_bounds[i] + current_layer->backsubstituted_lower_constraints[(i+1)*current_layer->max_coeffs - 1];    // adding the constant bias term
        }

        delete[] prev_bounds;


        /* Update constraints now */
        start = clock_start();
        // update lower constraints
        if (prev_layer->type == INPUT){
            ;
        } else if(prev_layer->type == RELU){
            update_lower_constraints_with_activation(current_layer, prev_layer);
        } else {
            if(DO_DP_BS){
                update_lower_constraints_with_affine(current_layer, prev_layer);        
            } else {
                update_lower_constraints_with_bsed_affine(current_layer, prev_layer);
            }
        }
        tt = time_from(start);

        if(prev_layer->type == RELU){
            current_layer->time_for_bs_using_act += tt;
        } else {
            current_layer->time_for_bs_using_affine += tt;
        }

    } else {
        cleartext_update_lower_bounds_using_prev_layers(current_layer, prev_layer);
    }

    
}

template <typename T>
void update_lower_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){

    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * prev_layer->max_coeffs];

    if constexpr (std::is_same<IntFp, T>::value && SECURE){
        double time_for_comp = 0;
        double time_for_ip = 0;

        assert((current_layer->max_coeffs - 1) == prev_layer->output_size);

        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        IntFp* prev_coeffs_to_mult = new IntFp[2 * prev_layer->output_size];
        T* copied_lc = new T[2*(current_layer->max_coeffs - 1)];

        for(int i = 0; i < current_layer->output_size; i++){
            T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
            
            ZKcmpPositive(current_layer->party, current_lower_constraints, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }


            for(int k = 0; k < prev_layer->max_coeffs; k++){


                for(int j = 0; j < prev_layer->output_size; j++){
                    prev_coeffs_to_mult[j]                           = prev_layer->lower_constraints[j * prev_layer->max_coeffs + k];
                    prev_coeffs_to_mult[j + prev_layer->output_size] = prev_layer->upper_constraints[j * prev_layer->max_coeffs + k];
                }

                // select prev layer LC or UC to multiply
                for(int j = 0; j < 2 * prev_layer->output_size; j++){
                    prev_coeffs_to_mult[j] = prev_coeffs_to_mult[j] * coeff_sign[j];
                }


                for(int j = 0; j < current_layer->max_coeffs-1; j++){
                    copied_lc[j]                                 = current_lower_constraints[j];
                    copied_lc[j + current_layer->max_coeffs - 1] = current_lower_constraints[j];
                }

                new_backsubstituted_lower_constraints[i*prev_layer->max_coeffs + k] = inner_product_bundle(2*prev_layer->output_size, copied_lc, prev_coeffs_to_mult, current_layer->party);//
            }

            ZKgeneralTruncAny(
                current_layer->party, 
                new_backsubstituted_lower_constraints + i*prev_layer->max_coeffs, 
                new_backsubstituted_lower_constraints + i*prev_layer->max_coeffs,
                prev_layer->max_coeffs,
                FXPSCALE
            );

            // adding constant term to constant product
            new_backsubstituted_lower_constraints[(i + 1)*prev_layer->max_coeffs - 1] = new_backsubstituted_lower_constraints[(i + 1)*prev_layer->max_coeffs - 1] 
                                                                                        + current_lower_constraints[current_layer->max_coeffs - 1];
        }

        delete[] coeff_sign;
        delete[] prev_coeffs_to_mult;
        delete[] copied_lc;

        // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
        // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

        current_layer->max_coeffs = prev_layer->max_coeffs; // check

        delete[] current_layer->backsubstituted_lower_constraints;
        current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;

    } else {
        cleartext_update_lower_constraints_with_affine(current_layer, prev_layer);
    }

}

template <typename T>
void update_lower_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * (prev_layer->input_size + 1)];

    if constexpr (std::is_same<IntFp, T>::value && SECURE) {

        // cleartext_update_lower_constraints_with_activation(current_layer, prev_layer);  return;


        assert((current_layer->max_coeffs - 1) == prev_layer->output_size);

        T* constant_terms = new T[current_layer->output_size];
        T* prev_layer_coeffs = new T[2 * prev_layer->output_size];
        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        T* copied_lc = new T[2*(current_layer->max_coeffs - 1)];
        T* non_constant_coeffs = new T[prev_layer->output_size];


        /* handle constant term */
        for(int i = 0; i < current_layer->output_size; i++){

            T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
            
            ZKcmpPositive(current_layer->party, current_lower_constraints, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }


            for(int j = 0; j < prev_layer->output_size; j++){
                prev_layer_coeffs[j]                           = prev_layer->lower_constraints[j*2 + 1];
                prev_layer_coeffs[j + prev_layer->output_size] = prev_layer->upper_constraints[j*2 + 1];
            }

            // select which prev LC or UC to multiply with
            for(int j = 0; j < 2*prev_layer->output_size; j++){
                prev_layer_coeffs[j] = prev_layer_coeffs[j] * coeff_sign[j];
            }

            for(int j = 0; j < current_layer->max_coeffs-1; j++){
                copied_lc[j]                                 = current_lower_constraints[j];
                copied_lc[j + current_layer->max_coeffs - 1] = current_lower_constraints[j];
            }

            constant_terms[i] = inner_product_bundle(2*prev_layer->output_size, copied_lc, prev_layer_coeffs, current_layer->party);

            new_backsubstituted_lower_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] = current_lower_constraints[current_layer->max_coeffs - 1]; 



            /* lookahead: for non-constant term */

            for(int j = 0; j < prev_layer->output_size; j++){
                non_constant_coeffs[j] =  prev_layer->lower_constraints[j*2 + 0] * coeff_sign[j]
                                        + prev_layer->upper_constraints[j*2 + 0] * coeff_sign[j + prev_layer->output_size]; 

                non_constant_coeffs[j] = non_constant_coeffs[j] * copied_lc[j];
            }

            ZKgeneralTruncAny(current_layer->party, non_constant_coeffs, non_constant_coeffs, prev_layer->output_size, FXPSCALE);

            for(int j = 0; j < prev_layer->output_size; j++){
                new_backsubstituted_lower_constraints[i * (prev_layer->input_size + 1) + j] = non_constant_coeffs[j];
            }
        }    

        ZKgeneralTruncAny(current_layer->party, constant_terms, constant_terms, current_layer->output_size, FXPSCALE);

        for(int i = 0; i < current_layer->output_size; i++){
            new_backsubstituted_lower_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] = new_backsubstituted_lower_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] +
                                                                                                constant_terms[i];
        }


        current_layer->max_coeffs = prev_layer->input_size + 1; // check
        
        delete[] current_layer->backsubstituted_lower_constraints;
        delete[] constant_terms;
        delete[] prev_layer_coeffs;
        delete[] coeff_sign;
        delete[] copied_lc;
        delete[] non_constant_coeffs;

        current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;

    } else {
        cleartext_update_lower_constraints_with_activation(current_layer, prev_layer);
    }
}

template <typename T>
void update_lower_constraints_with_bsed_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * 785];

    double time_for_comp = 0;
    double time_for_ip = 0;
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < 785; k++){        // including constant term
    
            auto start = clock_start();
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_lower_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_lower_constraints[j*785 + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_upper_constraints[j*785 + k];
                }    
            }
            double tt = time_from(start);
            time_for_comp += tt;

            start = clock_start();
            new_backsubstituted_lower_constraints[i*785 + k] = inner_product_emp(prev_layer->output_size, current_lower_constraints, prev_coeffs_to_mult);
            tt = time_from(start);
            time_for_ip += tt;
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(785, new_backsubstituted_lower_constraints + i*785, new_backsubstituted_lower_constraints + i*785);
        }

        // adding constant term to constant product
        new_backsubstituted_lower_constraints[(i + 1)*785 - 1] = new_backsubstituted_lower_constraints[(i + 1)*785 - 1] 
                                                                                    + current_lower_constraints[current_layer->max_coeffs - 1];
    }

    // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
    // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

    current_layer->max_coeffs = 785; // check

    delete[] current_layer->backsubstituted_lower_constraints;
    current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;
}



/* ===================== UPPER CONSTRAINTS ===================== */
template <typename T>
void update_upper_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
    assert(current_layer->max_coeffs == prev_layer->output_size + 1 && "Current layer's no. of coeffs should match (prev. layer's num neurons + 1).");
    auto start = clock_start();
    double tt;

    if constexpr (std::is_same<IntFp, T>::value && SECURE){
        T* prev_lbs = prev_layer->lower_bounds;
        T* prev_ubs = prev_layer->upper_bounds;

        // update upper bounds
        T* prev_bounds = new T[2*(current_layer->max_coeffs - 1)];       // lb_1 lb_2 ... lb_m   ub_1 ub_2 ... ub_m  
        for(int j = 0; j < current_layer->max_coeffs - 1; j++){
            prev_bounds[j]                                 = prev_ubs[j];
            prev_bounds[j + current_layer->max_coeffs - 1] = prev_lbs[j];
        }

        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        T* copied_uc = new T[2*(current_layer->max_coeffs - 1)];

        for(int i = 0; i < current_layer->output_size; i++){
        
            ZKcmpPositive(current_layer->party, current_layer->backsubstituted_upper_constraints + i*current_layer->max_coeffs, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }

            // first select which bound to multiply based on sign
            for(int j = 0; j < 2*(current_layer->max_coeffs - 1); j++){
                coeff_sign[j] = prev_bounds[j] * coeff_sign[j];
            }

            for(int j = 0; j < current_layer->max_coeffs-1; j++){
                copied_uc[j]                                 = current_layer->backsubstituted_upper_constraints[i*current_layer->max_coeffs + j];
                copied_uc[j + current_layer->max_coeffs - 1] = current_layer->backsubstituted_upper_constraints[i*current_layer->max_coeffs + j];
            }
            
            // inner product
            current_layer->upper_bounds[i] = inner_product_bundle(2*(current_layer->max_coeffs - 1), copied_uc, coeff_sign, current_layer->party);
        }

        // restore the fixed-point scale
        ZKgeneralTruncAny(current_layer->party, current_layer->upper_bounds, current_layer->upper_bounds, current_layer->output_size, FXPSCALE);

        for(int i = 0; i < current_layer->output_size; i++){
            current_layer->upper_bounds[i] = current_layer->upper_bounds[i] + current_layer->backsubstituted_upper_constraints[(i+1)*current_layer->max_coeffs - 1];    // adding the constant bias term
        }

        delete[] prev_bounds;
        delete[] copied_uc;
        delete[] coeff_sign;
    



        start = clock_start();
        // update lower constraints
        if (prev_layer->type == INPUT){
            ;
        } else if(prev_layer->type == RELU){
            update_upper_constraints_with_activation(current_layer, prev_layer);
        } else {
            if(DO_DP_BS){
                update_upper_constraints_with_affine(current_layer, prev_layer);        
            } else {
                update_upper_constraints_with_bsed_affine(current_layer, prev_layer);
            }
        }
        tt = time_from(start);

        if(prev_layer->type == RELU){
            current_layer->time_for_bs_using_act += tt;
        } else {
            current_layer->time_for_bs_using_affine += tt;
        }

    } else {
        cleartext_update_upper_bounds_using_prev_layers(current_layer, prev_layer);
    }
}

template <typename T>
void update_upper_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){

    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * prev_layer->max_coeffs];

    if constexpr (std::is_same<IntFp, T>::value && SECURE){
        double time_for_comp = 0;
        double time_for_ip = 0;

        assert((current_layer->max_coeffs - 1) == prev_layer->output_size);

        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        IntFp* prev_coeffs_to_mult = new IntFp[2 * prev_layer->output_size];
        T* copied_uc = new T[2*(current_layer->max_coeffs - 1)];

        for(int i = 0; i < current_layer->output_size; i++){
            T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);

            
            ZKcmpPositive(current_layer->party, current_upper_constraints, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }


            for(int k = 0; k < prev_layer->max_coeffs; k++){

                for(int j = 0; j < prev_layer->output_size; j++){
                    prev_coeffs_to_mult[j]                           = prev_layer->upper_constraints[j * prev_layer->max_coeffs + k];
                    prev_coeffs_to_mult[j + prev_layer->output_size] = prev_layer->lower_constraints[j * prev_layer->max_coeffs + k];
                }

                // select prev layer LC or UC to multiply
                for(int j = 0; j < 2 * prev_layer->output_size; j++){
                    prev_coeffs_to_mult[j] = prev_coeffs_to_mult[j] * coeff_sign[j];
                }

                for(int j = 0; j < current_layer->max_coeffs-1; j++){
                    copied_uc[j]                                 = current_upper_constraints[j];
                    copied_uc[j + current_layer->max_coeffs - 1] = current_upper_constraints[j];
                }

                new_backsubstituted_upper_constraints[i*prev_layer->max_coeffs + k] = inner_product_bundle(2*prev_layer->output_size, copied_uc, prev_coeffs_to_mult, current_layer->party);//
            }

            ZKgeneralTruncAny(
                current_layer->party, 
                new_backsubstituted_upper_constraints + i*prev_layer->max_coeffs, 
                new_backsubstituted_upper_constraints + i*prev_layer->max_coeffs,
                prev_layer->max_coeffs,
                FXPSCALE
            );

            // adding constant term to constant product
            new_backsubstituted_upper_constraints[(i + 1)*prev_layer->max_coeffs - 1] = new_backsubstituted_upper_constraints[(i + 1)*prev_layer->max_coeffs - 1] 
                                                                                        + current_upper_constraints[current_layer->max_coeffs - 1];
        }

        // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
        // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

        current_layer->max_coeffs = prev_layer->max_coeffs; // check

        delete[] current_layer->backsubstituted_upper_constraints;
        delete[] coeff_sign;
        delete[] prev_coeffs_to_mult;
        delete[] copied_uc;

        current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;

    } else {
        cleartext_update_upper_constraints_with_affine(current_layer, prev_layer);
    }

}

template <typename T>
void update_upper_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * (prev_layer->input_size + 1)];

    if constexpr (std::is_same<IntFp, T>::value && SECURE) {

        assert((current_layer->max_coeffs - 1) == prev_layer->output_size);

        T* constant_terms = new T[current_layer->output_size];
        IntFp* coeff_sign = new IntFp[2 * (current_layer->max_coeffs - 1)];   
        T* prev_layer_coeffs = new T[2 * prev_layer->output_size];
        T* copied_uc = new T[2*(current_layer->max_coeffs - 1)];
        T* non_constant_coeffs = new T[prev_layer->output_size];

        /* handle constant term */
        for(int i = 0; i < current_layer->output_size; i++){

            T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
            
            ZKcmpPositive(current_layer->party, current_upper_constraints, ZERO_COMP_CONSTANT, coeff_sign, current_layer->max_coeffs - 1);
            for(int j = 0; j < current_layer->max_coeffs - 1; j++){
                coeff_sign[j + current_layer->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
            }


            for(int j = 0; j < prev_layer->output_size; j++){
                prev_layer_coeffs[j]                           = prev_layer->upper_constraints[j*2 + 1];
                prev_layer_coeffs[j + prev_layer->output_size] = prev_layer->lower_constraints[j*2 + 1];
            }

            // select which prev LC or UC to multiply with
            for(int j = 0; j < 2*prev_layer->output_size; j++){
                prev_layer_coeffs[j] = prev_layer_coeffs[j] * coeff_sign[j];
            }

            for(int j = 0; j < current_layer->max_coeffs-1; j++){
                copied_uc[j]                                 = current_upper_constraints[j];
                copied_uc[j + current_layer->max_coeffs - 1] = current_upper_constraints[j];
            }

            constant_terms[i] = inner_product_bundle(2*prev_layer->output_size, copied_uc, prev_layer_coeffs, current_layer->party);

            new_backsubstituted_upper_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] = current_upper_constraints[current_layer->max_coeffs - 1]; 



            /* lookahead: for non-constant term */

            for(int j = 0; j < prev_layer->output_size; j++){
                non_constant_coeffs[j] =  prev_layer->upper_constraints[j*2 + 0] * coeff_sign[j]
                                        + prev_layer->lower_constraints[j*2 + 0] * coeff_sign[j + prev_layer->output_size]; 

                non_constant_coeffs[j] = non_constant_coeffs[j] * copied_uc[j];
            }

            ZKgeneralTruncAny(current_layer->party, non_constant_coeffs, non_constant_coeffs, prev_layer->output_size, FXPSCALE);

            for(int j = 0; j < prev_layer->output_size; j++){
                new_backsubstituted_upper_constraints[i * (prev_layer->input_size + 1) + j] = non_constant_coeffs[j];
            }
        }    

        ZKgeneralTruncAny(current_layer->party, constant_terms, constant_terms, current_layer->output_size, FXPSCALE);

        for(int i = 0; i < current_layer->output_size; i++){
            new_backsubstituted_upper_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] = new_backsubstituted_upper_constraints[(i + 1) * (prev_layer->input_size + 1) - 1] +
                                                                                                constant_terms[i];
        }


        current_layer->max_coeffs = prev_layer->input_size + 1; // check
        
        delete[] current_layer->backsubstituted_upper_constraints;
        delete[] coeff_sign;
        delete[] copied_uc;
        delete[] constant_terms;
        delete[] non_constant_coeffs;
        delete[] prev_layer_coeffs;
        
        current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;

    } else {
        cleartext_update_upper_constraints_with_activation(current_layer, prev_layer);
    }
}

template <typename T>
void update_upper_constraints_with_bsed_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * 785];

    double time_for_comp = 0;
    double time_for_ip = 0;
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < 785; k++){        // including constant term
    
            auto start = clock_start();
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_upper_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_upper_constraints[j*785 + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_lower_constraints[j*785 + k];
                }    
            }
            double tt = time_from(start);
            time_for_comp += tt;

            start = clock_start();
            new_backsubstituted_upper_constraints[i*785 + k] = inner_product_emp(prev_layer->output_size, current_upper_constraints, prev_coeffs_to_mult);
            tt = time_from(start);
            time_for_ip += tt;
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(785, new_backsubstituted_upper_constraints + i*785, new_backsubstituted_upper_constraints + i*785);
        }

        // adding constant term to constant product
        new_backsubstituted_upper_constraints[(i + 1)*785 - 1] = new_backsubstituted_upper_constraints[(i + 1)*785 - 1] 
                                                                                    + current_upper_constraints[current_layer->max_coeffs - 1];
    }

    // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
    // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

    current_layer->max_coeffs = 785; // check

    delete[] current_layer->backsubstituted_upper_constraints;
    current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;
}



/* ===================== CLEARTEXT SEMANTICS ===================== */

/* ===================== LOWER CONSTRAINTS ===================== */

template <typename T>
void cleartext_update_lower_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
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
        if(DO_DP_BS){
            update_lower_constraints_with_affine(current_layer, prev_layer);        
        } else {
            update_lower_constraints_with_bsed_affine(current_layer, prev_layer);
        }
    }
    tt = time_from(start);

    if(prev_layer->type == RELU){
        current_layer->time_for_bs_using_act += tt;
    } else {
        current_layer->time_for_bs_using_affine += tt;
    }
}

template <typename T>
void cleartext_update_lower_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
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
void cleartext_update_lower_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
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

template <typename T>
void cleartext_update_lower_constraints_with_bsed_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_lower_constraints = new T[current_layer->output_size * 785];

    double time_for_comp = 0;
    double time_for_ip = 0;
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_lower_constraints = current_layer->backsubstituted_lower_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < 785; k++){        // including constant term
    
            auto start = clock_start();
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_lower_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_lower_constraints[j*785 + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_upper_constraints[j*785 + k];
                }    
            }
            double tt = time_from(start);
            time_for_comp += tt;

            start = clock_start();
            new_backsubstituted_lower_constraints[i*785 + k] = inner_product_emp(prev_layer->output_size, current_lower_constraints, prev_coeffs_to_mult);
            tt = time_from(start);
            time_for_ip += tt;
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(785, new_backsubstituted_lower_constraints + i*785, new_backsubstituted_lower_constraints + i*785);
        }

        // adding constant term to constant product
        new_backsubstituted_lower_constraints[(i + 1)*785 - 1] = new_backsubstituted_lower_constraints[(i + 1)*785 - 1] 
                                                                                    + current_lower_constraints[current_layer->max_coeffs - 1];
    }

    // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
    // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

    current_layer->max_coeffs = 785; // check

    delete[] current_layer->backsubstituted_lower_constraints;
    current_layer->backsubstituted_lower_constraints = new_backsubstituted_lower_constraints;
}



/* ===================== UPPER CONSTRAINTS ===================== */
template <typename T>
void cleartext_update_upper_bounds_using_prev_layers(Layer<T>* current_layer, Layer<T>* prev_layer){
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
        cleartext_update_upper_constraints_with_activation(current_layer, prev_layer);
    } else {
        if(DO_DP_BS){
            cleartext_update_upper_constraints_with_affine(current_layer, prev_layer);        
        } else {
            cleartext_update_upper_constraints_with_bsed_affine(current_layer, prev_layer);
        }
    }
}

template <typename T>
void cleartext_update_upper_constraints_with_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
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
void cleartext_update_upper_constraints_with_activation(Layer<T>* current_layer, Layer<T>* prev_layer){
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

template <typename T>
void cleartext_update_upper_constraints_with_bsed_affine(Layer<T>* current_layer, Layer<T>* prev_layer){
    // cout << "AFFINE LAYER " << current_layer->layer_num << "::" << "PREV LAYER " << prev_layer->layer_num << "\n";
    T* new_backsubstituted_upper_constraints = new T[current_layer->output_size * 785];

    double time_for_comp = 0;
    double time_for_ip = 0;
    for(int i = 0; i < current_layer->output_size; i++){
        T* current_upper_constraints = current_layer->backsubstituted_upper_constraints + (i*current_layer->max_coeffs);
        T* prev_coeffs_to_mult = new T[prev_layer->output_size];
        
        for(int k = 0; k < 785; k++){        // including constant term
    
            auto start = clock_start();
            for(int j = 0; j < prev_layer->output_size; j++){
                if(greater_eq_zero<T>(current_upper_constraints[j], false)){
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_upper_constraints[j*785 + k];
                } else {
                    prev_coeffs_to_mult[j] = prev_layer->backsubstituted_lower_constraints[j*785 + k];
                }    
            }
            double tt = time_from(start);
            time_for_comp += tt;

            start = clock_start();
            new_backsubstituted_upper_constraints[i*785 + k] = inner_product_emp(prev_layer->output_size, current_upper_constraints, prev_coeffs_to_mult);
            tt = time_from(start);
            time_for_ip += tt;
        }

        if constexpr (std::is_same<IntFp, T>::value){
            normalize(785, new_backsubstituted_upper_constraints + i*785, new_backsubstituted_upper_constraints + i*785);
        }

        // adding constant term to constant product
        new_backsubstituted_upper_constraints[(i + 1)*785 - 1] = new_backsubstituted_upper_constraints[(i + 1)*785 - 1] 
                                                                                    + current_upper_constraints[current_layer->max_coeffs - 1];
    }

    // cout << "Time for Comparisons = " << time_for_comp/1e6 << " seconds\n";
    // cout << "Time for Inner-Product = " << time_for_ip/1e6 << " seconds\n\n\n";

    current_layer->max_coeffs = 785; // check

    delete[] current_layer->backsubstituted_upper_constraints;
    current_layer->backsubstituted_upper_constraints = new_backsubstituted_upper_constraints;
}

#endif