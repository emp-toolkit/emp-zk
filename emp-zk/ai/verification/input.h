#ifndef __INPUT_H__
#define __INPUT_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class Input : public Layer<T> {
    public:
    T epsilon;

    Input(int input_size, int output_size, int max_coeffs = 1, float epsilon = 0.1) : Layer<T>(input_size, output_size, max_coeffs){
        if(input_size != output_size){
            error("Input layer should have same input size and output size!\n");
        }

        if(max_coeffs == -1){
            max_coeffs = 1;
        }
        this->max_coeffs = max_coeffs;

        this->input = new T[input_size]; 
        this->output = new T[output_size];
        this->type = LAYER_TYPE::INPUT;

        this->lower_bounds = new T[output_size];
        this->upper_bounds = new T[output_size];
        
        this->lower_constraints = new T[output_size*this->max_coeffs];
        this->upper_constraints = new T[output_size*this->max_coeffs];

        this->epsilon = constant<T>(epsilon);

        this->is_backsubstituted = true;    // input layer is by default backsubstituted always
    }

    void set_epsilon(float epsilon){
        this->epsilon = constant<T>(epsilon);
    }

    void forward(Layer<T>* input_layer, Layer<T>* prev_layer, bool do_inference = true){
        assert(prev_layer == NULL && "Input layer should not have any input from a previous layer!\n");

        compute_lower_bounds();
        compute_lower_constraints();

        compute_upper_bounds();
        compute_upper_constraints();

        for(int i = 0; i < this->input_size; i++){
            this->output[i] = T(this->input[i]);
        }

        this->prev_layer = NULL;
    }


    void compute_lower_bounds(){
        // l_i = inp_i for input layer
        for(int i = 0; i <  this->input_size; i++){
            this->lower_bounds[i] = subtract(this->input[i], this->epsilon);
            if(!greater_eq_zero<T>(this->lower_bounds[i], false)){
                this->lower_bounds[i] = constant<T>(0);
            } 
        }
    }

    void compute_upper_bounds(){
        // u_i = inp_i for input layer
        for(int i = 0; i <  this->input_size; i++){
            this->upper_bounds[i] = T(this->input[i]) + this->epsilon;
            if(greater_eq<T>(this->upper_bounds[i], constant<T>(1), true)){
                this->upper_bounds[i] = constant<T>(1);
            }
        }
    }


    void compute_lower_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->input_size; i++){
            this->lower_constraints[i] = T(this->lower_bounds[i]);
        }
    }

    void compute_upper_constraints(){
        // l_i ≤ x_i ≤ u_i for input layer
        for(int i = 0; i <  this->input_size; i++){
            this->upper_constraints[i] = T(this->upper_bounds[i]);
        }
    }
    
    void backsubstitute(Layer<T>* input_layer){
        assert(input_layer == this && "Input layer mismatch!\n");
        this->is_backsubstituted = true;
    }


    void reset(){
        delete[] this->input;
        delete[] this->output;
        delete[] this->lower_bounds;
        delete[] this->upper_bounds;
        delete[] this->lower_constraints;
        delete[] this->upper_constraints;

        this->max_coeffs = 1;

        this->input = new T[this->input_size];  // +1 for bias
        this->output = new T[this->output_size];

        this->lower_bounds = new T[this->output_size];
        this->upper_bounds = new T[this->output_size];
        
        this->lower_constraints = new T[this->output_size*this->max_coeffs];
        this->upper_constraints = new T[this->output_size*this->max_coeffs];
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
        cout << "\n\n";
    }
};


#endif