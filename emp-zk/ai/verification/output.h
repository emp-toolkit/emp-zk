#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class Output : public Layer<T> {
    public:
    Layer<T>* prev_layer;
    int ground_truth;
    bool verified = false;

    Output(int input_size, int output_size, int max_coeffs = 2) : Layer<T>(input_size, output_size, max_coeffs){
        if(input_size != output_size){
            error("Input layer should have same input size and output size!\n");
        }

        if(max_coeffs == -1){
            max_coeffs = 2;
        }
        this->max_coeffs = max_coeffs;

        this->input = new T[input_size]; 
        this->output = new T[output_size];
        this->type = LAYER_TYPE::OUTPUT;

        this->lower_bounds = new T[output_size];
        this->upper_bounds = new T[output_size];
        
        this->lower_constraints = new T[output_size*this->max_coeffs];
        this->upper_constraints = new T[output_size*this->max_coeffs];
    }

    void forward(Layer<T>* input_layer, Layer<T>* prev_layer, bool do_inference = false){
        assert(prev_layer == NULL && "Input layer should not have any input from a previous layer!\n");
        this->prev_layer = prev_layer;

        for(int i = 0; i < this->input_size; i++){
            this->input[i] = T(prev_layer->output[i]);
        }

        compute_lower_bounds();
        compute_lower_constraints();

        compute_upper_bounds();
        compute_upper_constraints();

        if(do_inference){
            for(int i = 0; i < this->input_size; i++){
                this->output[i] = T(this->input[i]);
            }
        }

        int prediction = classify();
        if (prediction != ground_truth){
            cout << "PREDICTED CLASS: " << prediction << "; GROUND TRUTH = " << this->ground_truth << "\n";
            return;
        } else {
            cout << "PREDICTED CLASS: " << prediction << "\n";
        }
        
        verify(ground_truth);

        cout << "VERIFIED: " << (this->verified ? "YES" : "NO") << "\n\n"; 
    }


    void compute_lower_bounds(){
        for(int i = 0; i <  this->input_size; i++){
            this->lower_bounds[i] = T(this->prev_layer->lower_bounds[i]);
        }
    }

    void compute_upper_bounds(){
        for(int i = 0; i <  this->input_size; i++){
            this->upper_bounds[i] = T(this->prev_layer->upper_bounds[i]);
        }
    }


    void compute_lower_constraints(){
        ;
    }

    void compute_upper_constraints(){
        ;
    }

    void backsubstitute(Layer<T>* input_layer){
        if (DO_DP_BS){
            ;
        } else {
            if(!this->prev_layer->is_backsubstituted){  
                this->prev_layer->backsubstitute(input_layer);
            }

            this->forward(input_layer, this->prev_layer, true);

            this->is_backsubstituted = true;
        }
    }

    int classify(){
        int classification_result = -1;
        if constexpr (std::is_same<IntFp, T>::value){
            std::vector<std::pair<Integer, int>> sorted_logits;
            Integer* integer_logits = convert_field_rep_to_emp_Integer(this->output_size, this->output);

            int max_logit_pos = 0;
            Integer max_logit = integer_logits[max_logit_pos];
            for(int i = 1; i < this->output_size; i++){
                if((integer_logits[i] > max_logit).reveal<bool>()){
                    max_logit_pos = i;
                    max_logit = integer_logits[i];
                }
            }

            classification_result = max_logit_pos;

            // // sort logits to find classification output
            // for(int i = 0; i < this->output_size; i++){
            //     sorted_logits.push_back({integer_logits[i], i});
            // }
            // std::sort(
            //     sorted_logits.begin(), 
            //     sorted_logits.end(),
            //     [](const std::pair<Integer, int> &a, const std::pair<Integer, int> &b) {
            //         Integer a_Int = a.first;
            //         Integer b_Int = b.first;

            //         return (!a_Int.geq(b_Int)).reveal<bool>();
            //     }
            // );

            // classification_result = sorted_logits[this->output_size-1].second;
            
        } else {
            std::vector<std::pair<float, int>> sorted_logits;

            int max_logit_pos = 0;
            int max_logit = this->output[max_logit_pos];
            for(int i = 1; i < this->output_size; i++){
                if(this->output[i] > max_logit){
                    max_logit_pos = i;
                    max_logit = this->output[i];
                }
            }

            classification_result = max_logit_pos;
            
        }

        assert(
            ((classification_result >= 0) && (classification_result < this->output_size)) && 
            std::string("Predicted class must be between 0 and "+itoa(this->output_size)).c_str()
        );

        return classification_result;
    }

    void verify(int prediction){
        this->verified = true;
        if constexpr (std::is_same<IntFp, T>::value){
            Integer* integer_lbs = convert_field_rep_to_emp_Integer(this->output_size, this->lower_bounds);
            Integer* integer_ubs = convert_field_rep_to_emp_Integer(this->output_size, this->upper_bounds);

            Integer prediction_lb = integer_lbs[prediction];
            Integer prediction_ub = integer_ubs[prediction];
            
            for(int i = 0; i < this->output_size; i++){
                if(i == prediction){
                    continue;
                }
                
                // bool lb_in_interval = integer_lbs[i].geq(prediction_lb).reveal<bool>() 
                //                         && prediction_ub.geq(integer_lbs[i]).reveal<bool>(); 
                // if(lb_in_interval){
                //     this->verified = false; 
                //     break;
                // }

                bool ub_in_interval = integer_ubs[i].geq(prediction_lb).reveal<bool>() 
                                        && prediction_ub.geq(integer_ubs[i]).reveal<bool>();
                if(ub_in_interval){
                    this->verified = false; 
                    break;
                }
            }            
        } else {

            float prediction_lb = this->lower_bounds[prediction];
            float prediction_ub = this->upper_bounds[prediction];
            
            for(int i = 0; i < this->output_size; i++){
                if(i == prediction){
                    continue;
                }
                
                // bool lb_in_interval = (prediction_lb <= this->lower_bounds[i]) && (this->lower_bounds[i] <= prediction_ub);
                // if(lb_in_interval){
                //     this->verified = false; 
                //     break;
                // }

                bool ub_in_interval = (prediction_lb <= this->upper_bounds[i]) && (this->upper_bounds[i] <= prediction_ub);
                if(ub_in_interval){
                    this->verified = false; 
                    break;
                }
            }
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
        cout << "\n";

        cout << "Verified: " << (this->verified ? "YES" : "NO");
        cout << "\n\n";
    }
};


#endif