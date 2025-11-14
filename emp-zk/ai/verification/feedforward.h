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

    int num_layers;
    Layer<T>** layers;
    
    int num_inputs;

    VerifiableFeedForwardNeuralNetwork(int num_layers, Layer<T>** layers){
        this->num_layers = num_layers;
        this->layers = layers;

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

    void load_network_parameters(const char* param_file_path){
        int layer_offset = 0;
        for(int k = 0; k < num_layers; k++){
            Layer<T>* curr_layer = layers[k];
            if(curr_layer->type == AFFINE){
                curr_layer->param->init_param_matrix(param_file_path, layer_offset);
                layer_offset += curr_layer->param->num_parameters();
            } else {
                continue;
            }
        }
    }

    void load_weights_and_biases(const char* param_file_path){
        int layer_offset = 0;
        for(int k = 0; k < num_layers; k++){
            Layer<T>* curr_layer = layers[k];
            if(curr_layer->type == AFFINE){
                ((Affine<T>*) curr_layer)->param->read_weights_and_biases(param_file_path, layer_offset);
                layer_offset += ((Affine<T>*) curr_layer)->param->num_parameters();
            } else {
                continue;
            }
        }
    }


    void load_input(const char* input_file_path, int input_offset = 0){
        assert(layers != NULL && "Layers not yet initialized, cannot load inputs!");
        assert(layers[0]->type == INPUT && "The first layer should be an INPUT layer, cannot load inputs!");

        Layer<T>* input_layer = layers[0];
        
        if constexpr (std::is_same<T, IntFp>::value){
            float* raw_inputs = new float[input_layer->input_size];
            read_next_elements(input_layer->input_size, raw_inputs, input_offset, input_file_path);
            
            input_layer->input = convert_reals_to_field_rep(input_layer->input_size, raw_inputs);
            input_layer->output = convert_reals_to_field_rep(input_layer->output_size, raw_inputs);

            delete[] raw_inputs;
        } else if constexpr (std::is_same<T, float>::value){
            read_next_elements(input_layer->input_size, input_layer->input, input_offset, input_file_path);        
            read_next_elements(input_layer->output_size, input_layer->output, input_offset, input_file_path);  
        }

        this->num_inputs = input_layer->input_size;
    }

    void set_epsilon(float epsilon){
        assert(layers[0]->type == INPUT && "The first layer should be an INPUT layer, cannot set epsilon!\n");
        ((Input<T>*) layers[0])->set_epsilon(epsilon);
    }

    
    void reset(){
        for(int i = 0; i < this->num_layers; i++){
            ((Layer<T>*) this->layers[i])->reset();
        }
    }

    bool forward(bool do_backsubstitution = false, bool do_inference = true){
        Layer<T>* prev_layer = nullptr;
        for(int i = 0; i < num_layers; i++){
            // skip INPUT and OUTPUT layers
            layers[i]->layer_num = i+1;
            layers[i]->forward(prev_layer, do_inference);
            prev_layer = layers[i];
        }
        bool verification_result = ((Output<T>*) layers[this->num_layers-1])->verified;

        if (!verification_result && do_backsubstitution) {
            // perform backsubstitution
            cout << "couldn't verify... performing backsubstitution....\n";
            this->layers[this->num_layers - 1]->backsubstitute(this->layers[0]);
            verification_result = ((Output<T>*) layers[this->num_layers-1])->verified;
        }

        return verification_result;
    }

    void describe(bool print_parameters = true){
        for(int i = 0; i < num_layers; i++){
            cout << "LAYER " << (i+1) << "\n";
            layers[i]->describe(print_parameters);
        }
    }
};

#endif