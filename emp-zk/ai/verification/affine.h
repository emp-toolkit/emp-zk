#ifndef __AFFINE_H__
#define __AFFINE_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include "emp-zk/ai/utils.h"
#include "emp-zk/ai/inference/affine.h"
#include "emp-zk/ai/inference/normalize.h"
#include "emp-zk/emp-zk-math/ZKmath-functions.h"
#include "emp-zk/ai/secure-utils.h"

#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class Affine : public Layer<T> {
    public:

    Parameters<T>* param;
    
    Affine(int input_size, int output_size, int max_coeffs = -1, int party = PUBLIC) : Layer<T>(input_size, output_size, max_coeffs, party){
        if(max_coeffs == -1){
            max_coeffs = this->input_size+1;
        }
        this->max_coeffs = max_coeffs;

        this->input = new T[input_size+1];  // +1 for bias
        this->output = new T[output_size];
        this->param = new Parameters<T>(output_size, input_size);
        this->type = LAYER_TYPE::AFFINE;

        this->lower_bounds = new T[output_size];
        this->upper_bounds = new T[output_size];
        
        this->lower_constraints = new T[output_size*this->max_coeffs];
        this->upper_constraints = new T[output_size*this->max_coeffs];

        this->backsubstituted_lower_constraints= new T[output_size*this->max_coeffs];
        this->backsubstituted_upper_constraints= new T[output_size*this->max_coeffs];
    }

    void forward(Layer<T>* input_layer, Layer<T>* prev_layer, bool do_inference = true){
        this->prev_layer = prev_layer;

        // clone the input
        for(int i = 0; i < this->input_size; i++){
            this->input[i] = prev_layer->output[i];
        }

        // for bias
        if constexpr (std::is_same<T, IntFp>::value) {
            this->input[this->input_size] = IntFp(1 << FXPSCALE);
        } else if constexpr (std::is_same<T, float>::value) {
            this->input[this->input_size] = float(1);
        }

        assert((this->param != NULL && this->param->param_matrix != NULL) && "Parameters not initialized for AFFINE layer");
        
        auto start = clock_start();
        compute_lower_constraints();
        compute_lower_bounds();

        compute_upper_constraints();
        compute_upper_bounds();
        double tt = time_from(start);
        this->time_for_fp += tt;

        start = clock_start();
        backsubstitute(input_layer);
        tt = time_from(start);
        this->time_for_bs += tt;
    
        // cout << "Layer " << this->layer_num << " done!\n";

        if(do_inference){
            affine_layer(this->output_size, this->input_size, this->param->param_matrix, this->input, this->output);
            if constexpr (std::is_same<T, IntFp>::value){
                normalize(this->output_size, this->output, this->output);
            }
        }
    }


    void compute_lower_bounds(){
        T* prev_lbs = ((Layer<T>*) this->prev_layer)->lower_bounds;
        T* prev_ubs = ((Layer<T>*) this->prev_layer)->upper_bounds;

        if constexpr (std::is_same<IntFp, T>::value && SECURE){

            T* prev_bounds = new T[2*(this->max_coeffs - 1)];       // lb_1 lb_2 ... lb_m   ub_1 ub_2 ... ub_m  
            for(int j = 0; j < this->max_coeffs - 1; j++){
                prev_bounds[j] = prev_lbs[j];
                prev_bounds[j + this->max_coeffs - 1] = prev_ubs[j];
            }

            auto start = clock_start();

            IntFp* coeff_sign = new IntFp[2 * (this->max_coeffs - 1)]; 
            T* copied_lc = new T[2*(this->max_coeffs - 1)];

            for(int i = 0; i < this->output_size; i++){
                
                ZKcmpPositive(this->party, this->lower_constraints + i*this->max_coeffs, ZERO_COMP_CONSTANT, coeff_sign, this->max_coeffs - 1);
                
                for(int j = 0; j < this->max_coeffs - 1; j++){
                    coeff_sign[j + this->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
                }

                // first select which bound to multiply based on sign
                for(int j = 0; j < 2*(this->max_coeffs - 1); j++){
                    coeff_sign[j] = prev_bounds[j] * coeff_sign[j];
                }

                for(int j = 0; j < this->max_coeffs-1; j++){
                    copied_lc[j]                        = this->lower_constraints[i*this->max_coeffs + j];
                    copied_lc[j + this->max_coeffs - 1] = this->lower_constraints[i*this->max_coeffs + j];
                }
                
                // inner product
                this->lower_bounds[i] = inner_product_bundle(2*(this->max_coeffs - 1), copied_lc, coeff_sign, this->party);

            }

            delete[] copied_lc;
            delete[] coeff_sign;

            double tt = time_from(start);
            cout << "time for lb: " << tt << " microsec\n";


            // restore the fixed-point scale
            ZKgeneralTruncAny(this->party, this->lower_bounds, this->lower_bounds, this->output_size, FXPSCALE);

            for(int i = 0; i < this->output_size; i++){
                this->lower_bounds[i] = this->lower_bounds[i] + this->lower_constraints[(i+1)*this->max_coeffs - 1];    // adding the constant bias term
            }

            delete[] prev_bounds;

        } else {
            T* prev_bounds = new T[this->max_coeffs];

            for(int i = 0; i <  this->output_size; i++){
                for(int j = 0; j < this->max_coeffs-1; j++){
                    if(greater_eq_zero<T>(this->lower_constraints[i*this->max_coeffs + j], false)){
                        prev_bounds[j] = prev_lbs[j];
                    } else {
                        prev_bounds[j] = prev_ubs[j];
                    }
                }
                prev_bounds[this->max_coeffs-1] = constant<T>(1);
                
                this->lower_bounds[i] = inner_product_emp(this->max_coeffs, this->lower_constraints + i*(this->max_coeffs),  prev_bounds);
            }

            delete[] prev_bounds;
        }

    }

    void compute_upper_bounds(){
        T* prev_lbs = ((Layer<T>*) this->prev_layer)->lower_bounds;
        T* prev_ubs = ((Layer<T>*) this->prev_layer)->upper_bounds;

        if constexpr (std::is_same<IntFp, T>::value && SECURE){

            T* prev_bounds = new T[2*(this->max_coeffs - 1)];       // ub_1 ub_2 ... ub_m   lb_1 lb_2 ... lb_m  
            for(int j = 0; j < this->max_coeffs - 1; j++){
                prev_bounds[j] = prev_ubs[j];
                prev_bounds[j + this->max_coeffs - 1] = prev_lbs[j];
            }

            IntFp* coeff_sign = new IntFp[2 * (this->max_coeffs - 1)];   
            T* copied_uc = new T[2*(this->max_coeffs - 1)];

            for(int i = 0; i < this->output_size; i++){
            
                ZKcmpPositive(this->party, this->upper_constraints + i*this->max_coeffs, ZERO_COMP_CONSTANT, coeff_sign, this->max_coeffs - 1);
                for(int j = 0; j < this->max_coeffs - 1; j++){
                    coeff_sign[j + this->max_coeffs - 1] = FIELD_ONE + coeff_sign[j].negate();
                }

                // first select which bound to multiply based on sign
                for(int j = 0; j < 2*(this->max_coeffs - 1); j++){
                    coeff_sign[j] = prev_bounds[j] * coeff_sign[j];
                }

                for(int j = 0; j < this->max_coeffs-1; j++){
                    copied_uc[j]                        = this->upper_constraints[i*this->max_coeffs + j];
                    copied_uc[j + this->max_coeffs - 1] = this->upper_constraints[i*this->max_coeffs + j];
                }
                
                // inner product
                this->upper_bounds[i] = inner_product_bundle(2*(this->max_coeffs - 1), copied_uc, coeff_sign, this->party);

            }

            delete[] copied_uc;
            delete[] coeff_sign;

            // restore the fixed-point scale
            ZKgeneralTruncAny(this->party, this->upper_bounds, this->upper_bounds, this->output_size, FXPSCALE);

            for(int i = 0; i < this->output_size; i++){
                this->upper_bounds[i] = this->upper_bounds[i] + this->upper_constraints[(i+1)*this->max_coeffs - 1];    // adding the constant bias term
            }

            delete[] prev_bounds;
            
        } else {
            T* prev_bounds = new T[this->max_coeffs];

            for(int i = 0; i < this->output_size; i++){
                for(int j = 0; j < this->max_coeffs-1; j++){
                    if(greater_eq_zero<T>(this->upper_constraints[i*this->max_coeffs + j], false)){
                        prev_bounds[j] = prev_ubs[j];
                    } else {
                        prev_bounds[j] = prev_lbs[j];
                    }
                }
                prev_bounds[this->max_coeffs-1] = constant<T>(1);

                this->upper_bounds[i] = inner_product_emp(this->max_coeffs, this->upper_constraints + i*(this->max_coeffs),  prev_bounds);
            
            }

            delete[] prev_bounds;
        }

    }


    void compute_lower_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->output_size; i++){
            for(int j = 0; j < this->max_coeffs; j++){
                this->lower_constraints[i*this->max_coeffs + j] = (this->param->param_matrix[i*this->max_coeffs + j]);
            }
        }
    }

    void compute_upper_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->output_size; i++){
            for(int j = 0; j < this->max_coeffs; j++){
                this->upper_constraints[i*this->max_coeffs + j] = (this->param->param_matrix[i*this->max_coeffs + j]);
            }
        }
    }


    void backsubstitute(Layer<T>* input_layer){
        
        if(DO_DP_BS){
            // cout << "LAYER " << this->layer_num << "\n";
            for(int i = 0; i < this->output_size * this->max_coeffs; i++){
                this->backsubstituted_lower_constraints[i] = (this->lower_constraints[i]);
                this->backsubstituted_upper_constraints[i] = (this->upper_constraints[i]);
            }
        
            Layer<T>* prev_layer = this->prev_layer;
            while(prev_layer != NULL){
                update_lower_bounds_using_prev_layers(this, prev_layer);     
                prev_layer = prev_layer->prev_layer;
            }
            this->max_coeffs = this->input_size + 1;

            prev_layer = this->prev_layer;
            while(prev_layer != NULL){
                update_upper_bounds_using_prev_layers(this, prev_layer);        
                prev_layer = prev_layer->prev_layer;
            }
            this->max_coeffs = this->input_size + 1;

        } else {
            for(int i = 0; i < this->output_size * this->max_coeffs; i++){
                this->backsubstituted_lower_constraints[i] = T(this->lower_constraints[i]);
                this->backsubstituted_upper_constraints[i] = T(this->upper_constraints[i]);
            }
        
            Layer<T>* prev_layer = this->prev_layer;
            while(prev_layer != NULL){
                update_lower_bounds_using_prev_layers(this, prev_layer);
                if(prev_layer->type == AFFINE){
                    prev_layer = input_layer;
                } else {
                    prev_layer = prev_layer->prev_layer;
                }
            }
            this->max_coeffs = this->input_size + 1;

            prev_layer = this->prev_layer;
            while(prev_layer != NULL){
                update_upper_bounds_using_prev_layers(this, prev_layer);        
                if(prev_layer->type == AFFINE){
                    prev_layer = input_layer;
                } else {
                    prev_layer = prev_layer->prev_layer;
                }
            }
            this->max_coeffs = this->input_size + 1;
        }
    }


    void reset(){
        delete[] this->input;
        delete[] this->output;
        delete[] this->lower_bounds;
        delete[] this->upper_bounds;
        delete[] this->lower_constraints;
        delete[] this->upper_constraints;

        this->max_coeffs = this->input_size + 1;

        this->input = new T[this->input_size+1];  // +1 for bias
        this->output = new T[this->output_size];

        this->lower_bounds = new T[this->output_size];
        this->upper_bounds = new T[this->output_size];
        
        this->lower_constraints = new T[this->output_size*this->max_coeffs];
        this->upper_constraints = new T[this->output_size*this->max_coeffs];

        this->is_backsubstituted = false;

        this->backsubstituted_lower_constraints= new T[this->output_size*this->max_coeffs];
        this->backsubstituted_upper_constraints= new T[this->output_size*this->max_coeffs];
    }

    void describe(bool print_parameters = true, bool print_expressions = false){
        cout << "Type: " << get_layer_type(this->type) << "\n";
        if(print_parameters){
            cout << "Parameters:\n";
            param->print_parameters();
        }

        cout << "Inputs:\n";
        for(int i = 0; i < this->input_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(this->input[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << this->input[i] << " ";
            }
        }
        cout << "\n";
         
        cout << "Outputs:\n";
        for(int i = 0; i < this->output_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(this->output[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << this->output[i] << " ";
            }
        }
        cout << "\n";

        cout << "Lower Bounds:\n";
        for(int i = 0; i < this->output_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(this->lower_bounds[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << this->lower_bounds[i] << " ";
            }
        }
        cout << "\n";
         
        cout << "Upper Bounds:\n";
        for(int i = 0; i < this->output_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(this->upper_bounds[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << this->upper_bounds[i] << " ";
            }
        }

        if (print_expressions){
            cout << "\n";

            cout << "Lower Expression:\n";
            for(int i = 0; i < this->output_size; i++){
                cout << "N" << i+1 << ": ";
                for(int k = 0; k < NUM_FEATURES[CURR_DATASET]+1; k++){
                    if constexpr (std::is_same<T, IntFp>::value){
                        cout << format_EMP_IntFp(this->backsubstituted_lower_constraints[i*(NUM_FEATURES[CURR_DATASET]+1) + k], 1) << " ";
                    } else if constexpr (std::is_same<T, float>::value) {
                        cout << this->backsubstituted_lower_constraints[i*(NUM_FEATURES[CURR_DATASET]+1) + k] << " ";
                    }
                }
                
                cout << "\n";
            }
            cout << "\n";

            cout << "Upper Expression:\n";
            for(int i = 0; i < this->output_size; i++){
                cout << "N" << i+1 << ": ";
                for(int k = 0; k < NUM_FEATURES[CURR_DATASET]+1; k++){
                    if constexpr (std::is_same<T, IntFp>::value){
                        cout << format_EMP_IntFp(this->backsubstituted_upper_constraints[i*(NUM_FEATURES[CURR_DATASET]+1) + k], 1) << " ";
                    } else if constexpr (std::is_same<T, float>::value) {
                        cout << this->backsubstituted_upper_constraints[i*(NUM_FEATURES[CURR_DATASET]+1) + k] << " ";
                    }
                }
                
                cout << "\n";
            }
        }

        cout << "\n\n";
    }


    void cleartext_compute_lower_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->output_size; i++){
            for(int j = 0; j < this->max_coeffs; j++){
                this->lower_constraints[i*this->max_coeffs + j] = (this->param->param_matrix[i*this->max_coeffs + j]);
            }
        }
    }

    void cleartext_compute_upper_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->output_size; i++){
            for(int j = 0; j < this->max_coeffs; j++){
                this->upper_constraints[i*this->max_coeffs + j] = (this->param->param_matrix[i*this->max_coeffs + j]);
            }
        }
    }

    void cleartext_compute_lower_bounds(){
        T* prev_lbs = ((Layer<T>*) this->prev_layer)->lower_bounds;
        T* prev_ubs = ((Layer<T>*) this->prev_layer)->upper_bounds;

        for(int i = 0; i <  this->output_size; i++){
            T* prev_bounds = new T[this->max_coeffs];
            for(int j = 0; j < this->max_coeffs-1; j++){
                if(greater_eq_zero<T>(this->lower_constraints[i*this->max_coeffs + j], false)){
                    prev_bounds[j] = prev_lbs[j];
                } else {
                    prev_bounds[j] = prev_ubs[j];
                }
            }
            prev_bounds[this->max_coeffs-1] = constant<T>(1);
            
            this->lower_bounds[i] = inner_product_emp(this->max_coeffs, this->lower_constraints + i*(this->max_coeffs),  prev_bounds);
        }

        if(std::is_same<IntFp, T>::value){
            normalize(this->output_size, (IntFp*)this->lower_bounds, (IntFp*)this->lower_bounds);
        }
    }

    void cleartext_compute_upper_bounds(){
        T* prev_lbs = ((Layer<T>*) this->prev_layer)->lower_bounds;
        T* prev_ubs = ((Layer<T>*) this->prev_layer)->upper_bounds;

        for(int i = 0; i <  this->output_size; i++){
            T* prev_bounds = new T[this->max_coeffs];
            for(int j = 0; j < this->max_coeffs-1; j++){
                if(greater_eq_zero<T>(this->upper_constraints[i*this->max_coeffs + j], false)){
                    prev_bounds[j] = prev_ubs[j];
                } else {
                    prev_bounds[j] = prev_lbs[j];
                }
            }
            prev_bounds[this->max_coeffs-1] = constant<T>(1);
            
            this->upper_bounds[i] = inner_product_emp(this->max_coeffs, this->upper_constraints + i*(this->max_coeffs),  prev_bounds);
        }

        if(std::is_same<IntFp, T>::value){
            normalize(this->output_size, (IntFp*)this->upper_bounds, (IntFp*)this->upper_bounds);
        }
    }



    
    void backsubstitute_lower_constraints(int num_inputs){
        T* new_lower_constraints = new T[this->output_size * this->max_coeffs];

        T* prev_lc = this->prev_layer->lower_constraints;
        T* prev_uc = this->prev_layer->upper_constraints;
        int num_neurons_in_prev_layer = this->input_size;

        for(int i = 0; i < this->output_size; i++){
            for(int k = 0; k < num_inputs; k++){
                new_lower_constraints[i*this->max_coeffs + k] = constant<T>(0);
                for(int j = 0; j < num_neurons_in_prev_layer; j++){
                    if(greater_eq_zero<T>((T)this->lower_constraints[i*(this->input_size+1) + j], false)){
                        new_lower_constraints[i*this->max_coeffs + k] = new_lower_constraints[i*this->max_coeffs + k] + this->lower_constraints[i*(this->input_size+1) + j]*prev_lc[j*(num_inputs + 1) + k];
                    } else {
                        new_lower_constraints[i*this->max_coeffs + k] = new_lower_constraints[i*this->max_coeffs + k] + this->lower_constraints[i*(this->input_size+1) + j]*prev_uc[j*(num_inputs + 1) + k];
                    }                 
                }
            }

            new_lower_constraints[(i+1)*this->max_coeffs - 1] = constant<T>(0);
            for(int j = 0; j < num_neurons_in_prev_layer; j++){
                if(greater_eq_zero<T>((T)this->lower_constraints[i*(this->input_size+1) + j], false)){
                    new_lower_constraints[(i+1)*this->max_coeffs - 1] = new_lower_constraints[(i+1)*this->max_coeffs - 1] + this->lower_constraints[i*(this->input_size+1) + j]*prev_lc[(j+1)*(num_inputs + 1) - 1];
                } else {
                    new_lower_constraints[(i+1)*this->max_coeffs - 1] = new_lower_constraints[(i+1)*this->max_coeffs - 1] + this->lower_constraints[i*(this->input_size+1) + j]*prev_uc[(j+1)*(num_inputs + 1) - 1];
                }                    
            }

            if(std::is_same<IntFp, T>::value){
                normalize(this->max_coeffs, (IntFp*) (new_lower_constraints + i*this->max_coeffs), (IntFp*) (new_lower_constraints + i*this->max_coeffs));
            }

            new_lower_constraints[(i + 1)*this->max_coeffs - 1] = new_lower_constraints[(i + 1)*this->max_coeffs - 1] + this->lower_constraints[(i + 1)*(this->input_size + 1) - 1];
        }
        // delete[] this->lower_constraints;
        this->lower_constraints = new_lower_constraints;
    }


    void backsubstitute_upper_constraints(int num_inputs){
        T* new_upper_constraints = new T[this->output_size * this->max_coeffs];

        T* prev_lc = this->prev_layer->lower_constraints;
        T* prev_uc = this->prev_layer->upper_constraints;
        int num_neurons_in_prev_layer = this->input_size;

        for(int i = 0; i < this->output_size; i++){
            for(int k = 0; k < num_inputs; k++){
                new_upper_constraints[i*this->max_coeffs + k] = constant<T>(0);
                for(int j = 0; j < num_neurons_in_prev_layer; j++){
                    if(greater_eq_zero<T>((T)this->upper_constraints[i*(this->input_size+1) + j], false)){
                        new_upper_constraints[i*this->max_coeffs + k] = new_upper_constraints[i*this->max_coeffs + k] + this->upper_constraints[i*(this->input_size+1) + j]*prev_uc[j*(num_inputs + 1) + k];
                    } else {
                        new_upper_constraints[i*this->max_coeffs + k] = new_upper_constraints[i*this->max_coeffs + k] + this->upper_constraints[i*(this->input_size+1) + j]*prev_lc[j*(num_inputs + 1) + k];
                    }                    
                }
            }

            new_upper_constraints[(i+1)*this->max_coeffs - 1] = constant<T>(0);
            for(int j = 0; j < num_neurons_in_prev_layer; j++){
                if(greater_eq_zero<T>((T)this->upper_constraints[i*(this->input_size+1) + j], false)){
                    new_upper_constraints[(i+1)*this->max_coeffs - 1] = new_upper_constraints[(i+1)*this->max_coeffs - 1] + this->upper_constraints[i*(this->input_size+1) + j]*prev_uc[(j+1)*(num_inputs + 1) - 1];
                } else {
                    new_upper_constraints[(i+1)*this->max_coeffs - 1] = new_upper_constraints[(i+1)*this->max_coeffs - 1] + this->upper_constraints[i*(this->input_size+1) + j]*prev_lc[(j+1)*(num_inputs + 1) - 1];
                }                    
            }

            if(std::is_same<IntFp, T>::value){
                normalize(this->max_coeffs, (IntFp*) (new_upper_constraints + i*this->max_coeffs), (IntFp*) (new_upper_constraints + i*this->max_coeffs));
            }

            new_upper_constraints[(i + 1)*this->max_coeffs - 1] = new_upper_constraints[(i + 1)*this->max_coeffs - 1] + this->upper_constraints[(i + 1)*(this->input_size + 1) - 1];
        }

        // delete[] this->upper_constraints;
        this->upper_constraints = new_upper_constraints;
    }

    void compute_lower_bounds_after_backsubstitution(Layer<T>* input_layer){
        T* input_lb = input_layer->lower_bounds;
        T* input_ub = input_layer->upper_bounds;
        int num_inputs = input_layer->input_size;

        for(int i = 0; i < this->output_size; i++){
            this->lower_bounds[i] = constant<T>(0);
            for(int k = 0; k < num_inputs; k++){
                if(greater_eq_zero(this->lower_constraints[i*this->max_coeffs + k], false)){
                    this->lower_bounds[i] = this->lower_bounds[i] + this->lower_constraints[i*this->max_coeffs + k]*input_lb[k];
                } else {
                    this->lower_bounds[i] = this->lower_bounds[i] + this->lower_constraints[i*this->max_coeffs + k]*input_ub[k];
                }
            }
        }

        if constexpr (std::is_same<T, IntFp>::value){
            normalize(this->output_size, (IntFp*) this->lower_bounds, (IntFp*) this->lower_bounds);
        }

        for(int i = 0; i < this->output_size; i++){
            this->lower_bounds[i] = this->lower_bounds[i] + this->lower_constraints[(i+1)*this->max_coeffs - 1];
        }
    }

    void compute_upper_bounds_after_backsubstitution(Layer<T>* input_layer){
        T* input_lb = input_layer->lower_bounds;
        T* input_ub = input_layer->upper_bounds;
        int num_inputs = input_layer->input_size;

        for(int i = 0; i < this->output_size; i++){
            this->upper_bounds[i] = constant<T>(0);
            for(int k = 0; k < num_inputs; k++){
                if(greater_eq_zero(this->upper_constraints[i*this->max_coeffs + k], false)){
                    this->upper_bounds[i] = this->upper_bounds[i] + this->upper_constraints[i*this->max_coeffs + k]*input_ub[k];
                } else {
                    this->upper_bounds[i] = this->upper_bounds[i] + this->upper_constraints[i*this->max_coeffs + k]*input_lb[k];
                }
            }
        }

        if constexpr (std::is_same<T, IntFp>::value){
            normalize(this->output_size, (IntFp*) this->upper_bounds, (IntFp*) this->upper_bounds);
        }

        for(int i = 0; i < this->output_size; i++){
            this->upper_bounds[i] = this->upper_bounds[i] + this->upper_constraints[(i+1)*this->max_coeffs - 1];
        }
    }

};


#endif