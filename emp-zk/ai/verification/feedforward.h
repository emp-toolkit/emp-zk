#ifndef __VERIFIABLE_FEEDFORWARD_H__
#define __VERIFIABLE_FEEDFORWARD_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include "emp-zk/ai/utils.h"
#include <iostream>

using namespace emp;
using namespace std;


template <typename T>
class VerifiableFeedForwardNeuralNetwork {
    public:
    int party = PUBLIC;
    int num_layers;
    Layer<T>** layers;
    
    int num_inputs;

    VerifiableFeedForwardNeuralNetwork(int num_layers, Layer<T>** layers, int party = PUBLIC){
        this->num_layers = num_layers;
        this->layers = layers;
        this->party = party;
        // validate_layers();
    }

    void validate_layers(){
        assert(num_layers > 0 && "Network must have at least 1 layer\n");

        assert(layers[0]->type == INPUT && "The first layer of the network must be an INPUT layer\n");

        for(int i = 1; i < num_layers-1; i += 2){
            assert(
                (layers[i]->type == AFFINE && layers[i+1]->type == RELU) && 
                "Intermediate layers must alternate with AFFINE and RELU\n"
            );
        }

        assert(layers[num_layers-1]->type == OUTPUT &&  "The last layer of the network must be an OUTPUT layer\n");


        // validate layer sizes
        for(int i = 1; i < num_layers; i++){
            Layer<T>* prev_layer = layers[i-1];
            assert(
                prev_layer->output_size == layers[i]->input_size && 
                "Output size of previous layer must match input size of current layer"
            );

            prev_layer = layers[i];
        }
    }

    void load_weights_and_biases(const char* param_file_path){
        int layer_offset = 0;
        for(int k = 0; k < num_layers; k++){
            Layer<T>* curr_layer = layers[k];
            if(curr_layer->type == AFFINE){
                ((Affine<T>*) curr_layer)->param->read_weights_and_biases(param_file_path, layer_offset);
                layer_offset += ((Affine<T>*) curr_layer)->param->num_parameters();
            }
        }
    }

    void load_input(const char* input_file_path, int input_offset = 0, float epsilon = 0.1){
        assert(layers != NULL && "Layers not yet initialized, cannot load inputs!");
        assert(layers[0]->type == INPUT && "The first layer should be an INPUT layer, cannot load inputs!");

        Layer<T>* input_layer = layers[0];

        // read ground truth
        float gt;
        read_next_elements(1, &gt, input_offset, input_file_path);
        ((Output<T>*) layers[num_layers-1])->ground_truth = int(gt);
        input_offset++;
        
        
        // read inputs
        float* raw_inputs = new float[input_layer->input_size];
        read_next_elements(input_layer->input_size, raw_inputs, input_offset, input_file_path);

        float* raw_lb = new float[input_layer->input_size];
        for(int i = 0; i < input_layer->input_size; i++){
            raw_lb[i] = raw_inputs[i] - epsilon;
            if(raw_lb[i] < INPUT_MIN){
                raw_lb[i] = INPUT_MIN;
            }

            if(raw_lb[i] > INPUT_MAX){
                raw_lb[i] = INPUT_MAX;
            }
        }

        float* raw_ub = new float[input_layer->input_size];
        for(int i = 0; i < input_layer->input_size; i++){
            raw_ub[i] = raw_inputs[i] + epsilon;
            if(raw_ub[i] < INPUT_MIN){
                raw_ub[i] = INPUT_MIN;
            }

            if(raw_ub[i] > INPUT_MAX){
                raw_ub[i] = INPUT_MAX;
            }
        }
            
        if constexpr (std::is_same<T, IntFp>::value){
            
            input_layer->input = convert_reals_to_field_rep(input_layer->input_size, raw_inputs, PUBLIC);
            input_layer->output = convert_reals_to_field_rep(input_layer->output_size, raw_inputs, PUBLIC);
            input_layer->lower_bounds = convert_reals_to_field_rep(input_layer->input_size, raw_lb, PUBLIC);
            input_layer->upper_bounds = convert_reals_to_field_rep(input_layer->input_size, raw_ub, PUBLIC);

        } else if constexpr (std::is_same<T, float>::value){
            for(int i = 0; i < input_layer->input_size; i++){
                input_layer->input[i] = raw_inputs[i];
                input_layer->output[i] = raw_inputs[i];
                input_layer->lower_bounds[i] = raw_lb[i];
                input_layer->upper_bounds[i] = raw_ub[i];
            }
        }

        this->num_inputs = input_layer->input_size;


        delete[] raw_inputs;
        delete[] raw_lb;
        delete[] raw_ub;
    }
    
    void reset(){
        for(int i = 0; i < this->num_layers; i++){
            ((Layer<T>*) this->layers[i])->reset();
        }
    }


    bool forward(bool do_backsubstitution = false, bool do_inference = true){
        Layer<T>* prev_layer = nullptr;
        Layer<T>* input_layer = layers[0];

        for(int i = 0; i < num_layers; i++){
            layers[i]->layer_num = i+1;
            layers[i]->forward(input_layer, prev_layer, do_inference);
            prev_layer = layers[i];
        }
        
        bool verification_result;
        verification_result = ((Output<T>*) layers[this->num_layers-1])->verified;


        if(DO_DP_BS){
            // for(int i = 0; i < num_layers; i++){
            //     profiling(layers[i]);
            // }
        } else {
            // if (!verification_result && do_backsubstitution) {
            //     // perform backsubstitution
            //     cout << "couldn't verify... performing backsubstitution....\n";
            //     this->layers[this->num_layers - 1]->backsubstitute(this->layers[0]);
            //     verification_result = ((Output<T>*) layers[this->num_layers-1])->verified;
            // }
        }

        layers[num_layers - 1]->describe(false, false);

        return verification_result;
    }

    void describe(bool print_parameters = true, bool print_expressions = false){
        for(int i = 0; i < num_layers; i++){
            cout << "LAYER " << (i+1) << "\n";
            layers[i]->describe(print_parameters, print_expressions);
        }
    }
};

template <typename T>
void profiling(Layer<T>* layer){
    // if(layer->type != AFFINE){
    //     return;
    // }
    cout << "==============================================================================\n";
    cout << "Layer " << layer->layer_num << ":: " << get_layer_type(layer->type) << "\n";
    cout << "Time for FP: " << layer->time_for_fp/1e6 << " seconds\n";
    cout << "Time for BS: " << layer->time_for_bs/1e6 << " seconds\n";
    cout << "|\n";
    cout << "-----> Bound Computation: " << layer->time_for_bs_in_bounds/1e6 << " seconds\n|\n";
    cout << "-----> Update using ReLU: " << layer->time_for_bs_using_act/1e6 << " seconds\n|\n";
    cout << "-----> Update using Affn: " << layer->time_for_bs_using_affine/1e6 << " seconds\n";

    cout << "==============================================================================\n\n";
}

#endif