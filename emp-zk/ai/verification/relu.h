#ifndef __RELU_H__
#define __RELU_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include "emp-zk/ai/utils.h"

#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class ReLU : public Layer<T> {
    public:
    
    ReLU(int input_size, int output_size, int max_coeffs = 2) : Layer<T>(input_size, output_size, max_coeffs){
        if(input_size != output_size){
            error("ReLU layer should have same input size and output size!\n");
        }

        if(max_coeffs == -1){
            max_coeffs = 2;
        }
        this->max_coeffs = max_coeffs;

        this->input = new T[input_size]; 
        this->output = new T[output_size];
        this->type = RELU;

        this->lower_bounds = new T[output_size];
        this->upper_bounds = new T[output_size];
        
        this->lower_constraints = new T[output_size*this->max_coeffs];
        this->upper_constraints = new T[output_size*this->max_coeffs];
    }

    void forward(Layer<T>* input_layer, Layer<T>* prev_layer, bool do_inference = true){
        this->prev_layer = prev_layer;

        for(int i = 0; i < this->input_size; i++){
            this->input[i] = T(prev_layer->output[i]);
        }

        auto start = clock_start();
        compute_lower_constraints();
        compute_lower_bounds();
        
        compute_upper_constraints();
        compute_upper_bounds();
        double tt = time_from(start);
        this->time_for_fp += tt;

        if(DO_DP_BS){
            backsubstitute(input_layer);
        }

        if(do_inference){
            relu_layer(this->input_size, this->input, this->output);
        }
    }

    void compute_lower_bounds(){
        for(int i = 0; i < this->output_size; i++){
            T prev_lb = this->prev_layer->lower_bounds[i];

            if(greater_eq_zero<T>(prev_lb, false)){
                this->lower_bounds[i] = T(prev_lb);
            } else {
                this->lower_bounds[i] = constant<T>(0);
            }

            /*
            if(greater_eq_zero<T>(prev_lb, false)){
                this->lower_bounds[i] = T(prev_lb);
            } else if(!greater_eq_zero<T>(prev_ub, false)){
                this->lower_bounds[i] = constant<T>(0);
            } else {
                // l^(k-1)_i < 0 && u^(k-1)_i > 0
                T abs_diff = prev_ub + prev_lb;
                if(greater_eq_zero(abs_diff, false)){
                    // |u_i| > |l_i|
                    // this->lower_bounds[i] = T(prev_lb);
                } else {
                    // |u_i| < |l_i|
                    this->lower_bounds[i] = constant<T>(0);
                }
            }
            */
        }
    }

    void compute_upper_bounds(){
        for(int i = 0; i < this->output_size; i++){
            T prev_ub = this->prev_layer->upper_bounds[i];

            if(greater_eq_zero<T>(prev_ub, false)){
                this->upper_bounds[i] = T(prev_ub);
            } else {
                this->upper_bounds[i] = constant<T>(0);
            }

            /*
            if(greater_eq_zero<T>(prev_lb, false)){
                this->upper_bounds[i] = T(prev_ub);
            } else if(!greater_eq_zero<T>(prev_ub, false)){
                this->upper_bounds[i] = constant<T>(0);
            } else {
                // l^(k-1)_i < 0 && u^(k-1)_i > 0
                this->upper_bounds[i] = T(prev_ub);
            }
            */
        }
    }


    void compute_lower_constraints(){
        for(int i = 0; i < this->output_size; i++){
            T prev_lb = this->prev_layer->lower_bounds[i];
            T prev_ub = this->prev_layer->upper_bounds[i];

            if(greater_eq_zero<T>(prev_lb, false)){
                this->lower_constraints[i*2 + 0] = constant<T>(1);
                this->lower_constraints[i*2 + 1] = constant<T>(0);
            } else if(!greater_eq_zero<T>(prev_ub, false)){
                this->lower_constraints[i*2 + 0] = constant<T>(0);
                this->lower_constraints[i*2 + 1] = constant<T>(0);
            } else {
                // l^(k-1)_i < 0 && u^(k-1)_i > 0
                T abs_diff = prev_ub + prev_lb;
                if(greater_eq_zero(abs_diff, false)){
                    // |u_i| > |l_i|
                    // y = x
                    this->lower_constraints[i*2 + 0] = constant<T>(1);
                    this->lower_constraints[i*2 + 1] = constant<T>(0);
                } else {
                    // |u_i| < |l_i|
                    // y = 0
                    this->lower_constraints[i*2 + 0] = constant<T>(0);
                    this->lower_constraints[i*2 + 1] = constant<T>(0);
                }
            }
        }
    }
       
    void compute_upper_constraints(){
        for(int i = 0; i < this->output_size; i++){
            T prev_lb = this->prev_layer->lower_bounds[i];
            T prev_ub = this->prev_layer->upper_bounds[i];

            if(greater_eq_zero<T>(prev_lb, false)){
                this->upper_constraints[i*2 + 0] = constant<T>(1);
                this->upper_constraints[i*2 + 1] = constant<T>(0);
            } else if(!greater_eq_zero<T>(prev_ub, false)){
                this->upper_constraints[i*2 + 0] = constant<T>(0);
                this->upper_constraints[i*2 + 1] = constant<T>(0);
            } else {
                // l^(k-1)_i < 0 && u^(k-1)_i > 0
                T abs_diff = prev_ub + prev_lb;

                // |u_i| > |l_i|
                this->upper_constraints[i*2 + 0] = divide(prev_ub, subtract(prev_ub, prev_lb));

                T prod = prev_lb*prev_ub;
                if(std::is_same<IntFp, T>::value){
                    normalize(1, (IntFp*)&prod, (IntFp*)&prod);
                }
                this->upper_constraints[i*2 + 1] = divide(prod, subtract(prev_lb, prev_ub));
            
            }
        }
    }


    void backsubstitute(Layer<T>* input_layer){
        if(DO_DP_BS){
            ;
        } else {
            if(!this->prev_layer->is_backsubstituted){
                this->prev_layer->backsubstitute(input_layer);
            }  

            int num_inputs = input_layer->input_size;
            this->max_coeffs = num_inputs + 1;

            this->backsubstitute_lower_constraints(num_inputs);
            this->backsubstitute_upper_constraints(num_inputs);
            this->is_backsubstituted = true;

            // this->compute_lower_bounds_after_backsubstitution(input_layer);
            // this->compute_upper_bounds_after_backsubstitution(input_layer);
        
            this->compute_lower_bounds();
            this->compute_upper_bounds();
        }
    }

    void reset(){
        delete[] this->input;
        delete[] this->output;
        delete[] this->lower_bounds;
        delete[] this->upper_bounds;
        delete[] this->lower_constraints;
        delete[] this->upper_constraints;

        this->max_coeffs = 2;

        this->input = new T[this->input_size];  
        this->output = new T[this->output_size];

        this->lower_bounds = new T[this->output_size];
        this->upper_bounds = new T[this->output_size];
        
        this->lower_constraints = new T[this->output_size*this->max_coeffs];
        this->upper_constraints = new T[this->output_size*this->max_coeffs];

        this->is_backsubstituted = false;
    }


    void describe(bool print_parameters = false, bool print_expressions = false){
        cout << "Type: " << get_layer_type(this->type) << "\n";
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
                for(int k = 0; k < this->max_coeffs; k++){
                    if constexpr (std::is_same<T, IntFp>::value){
                        cout << format_EMP_IntFp(this->lower_constraints[i*this->max_coeffs + k], 1) << " ";
                    } else if constexpr (std::is_same<T, float>::value) {
                        cout << this->lower_constraints[i*this->max_coeffs + k] << " ";
                    }
                }
                
                cout << "\n";
            }
            cout << "\n";

            cout << "Upper Expression:\n";
            for(int i = 0; i < this->output_size; i++){
                cout << "N" << i+1 << ": ";
                for(int k = 0; k < this->max_coeffs; k++){
                    if constexpr (std::is_same<T, IntFp>::value){
                        cout << format_EMP_IntFp(this->upper_constraints[i*this->max_coeffs + k], 1) << " ";
                    } else if constexpr (std::is_same<T, float>::value) {
                        cout << this->upper_constraints[i*this->max_coeffs + k] << " ";
                    }
                }
                
                cout << "\n";
            }
        }

        cout << "\n\n";
    }

    /*
    void backsubstitute_lower_constraints(int num_inputs){
        T* new_lower_constraints = new T[this->output_size*this->max_coeffs];

        T* prev_lc = this->prev_layer->lower_constraints;
        T* prev_uc = this->prev_layer->upper_constraints;

        for(int i = 0; i < this->output_size; i++){ // for each neuron in the layer
            for(int k = 0; k < num_inputs; k++){
                if(greater_eq_zero<T>((T)this->lower_constraints[2*i + 0], false)){
                    // replace by lower coefficient
                    new_lower_constraints[i*this->max_coeffs + k] = 
                                                    this->lower_constraints[2*i + 0]
                                                    *prev_lc[i*(num_inputs+1) + k];
                } else {
                    // replace by upper coefficient
                    new_lower_constraints[i*this->max_coeffs + k] = 
                                                    this->lower_constraints[2*i + 0]
                                                    *prev_uc[i*(num_inputs+1) + k];
                }
            }


            if(greater_eq_zero<T>((T)this->lower_constraints[2*i + 0], false)){
                // replace by lower coefficient
                new_lower_constraints[(i+1)*this->max_coeffs - 1] = 
                                                this->lower_constraints[2*i + 0]
                                                *prev_lc[(i+1)*(num_inputs+1) - 1];
            } else {
                // replace by upper coefficient
                new_lower_constraints[(i+1)*this->max_coeffs - 1] = 
                                                this->lower_constraints[2*i + 0]
                                                *prev_uc[(i+1)*(num_inputs+1) - 1];
            }


            if(std::is_same<IntFp, T>::value){
                normalize(this->max_coeffs, (IntFp*) (new_lower_constraints + i*this->max_coeffs), (IntFp*) (new_lower_constraints + i*this->max_coeffs));
            }

            new_lower_constraints[(i + 1) * this->max_coeffs - 1] = new_lower_constraints[(i + 1) * this->max_coeffs - 1] + this->lower_constraints[2*i + 1];
        }

        // delete[] this->lower_constraints;
        this->lower_constraints = new_lower_constraints;
    }

    void backsubstitute_upper_constraints(int num_inputs){
        T* new_upper_constraints = new T[this->output_size*this->max_coeffs];

        T* prev_lc = this->prev_layer->lower_constraints;
        T* prev_uc = this->prev_layer->upper_constraints;

        for(int i = 0; i < this->output_size; i++){ // for each neuron in the layer
            for(int k = 0; k < num_inputs; k++){
                if(greater_eq_zero<T>((T)this->upper_constraints[2*i + 0], false)){
                    // replace by upper coefficient
                    new_upper_constraints[i*this->max_coeffs + k] = 
                                                    this->upper_constraints[2*i + 0]
                                                    *prev_uc[i*(num_inputs+1) + k];
                } else {
                    // replace by lower coefficient
                    new_upper_constraints[i*this->max_coeffs + k] = 
                                                    this->upper_constraints[2*i + 0]
                                                    *prev_lc[i*(num_inputs+1) + k];
                }
            }

            if(greater_eq_zero<T>((T)this->upper_constraints[2*i + 0], false)){
                // replace by upper coefficient
                new_upper_constraints[(i+1)*this->max_coeffs - 1] = 
                                                this->upper_constraints[2*i + 0]
                                                *prev_uc[(i+1)*(num_inputs+1) - 1];
            } else {
                // replace by lower coefficient
                new_upper_constraints[(i+1)*this->max_coeffs - 1] = 
                                                this->upper_constraints[2*i + 0]
                                                *prev_lc[(i+1)*(num_inputs+1) - 1];
            }


            if(std::is_same<IntFp, T>::value){
                normalize(this->max_coeffs, (IntFp*) (new_upper_constraints + i*this->max_coeffs), (IntFp*) (new_upper_constraints + i*this->max_coeffs));
            }

            new_upper_constraints[(i + 1) * this->max_coeffs - 1] = new_upper_constraints[(i + 1) * this->max_coeffs - 1] + this->upper_constraints[2*i + 1];
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
    */
};


#endif