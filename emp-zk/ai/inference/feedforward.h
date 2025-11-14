#ifndef __FEEDFORWARD_H__
#define __FEEDFORWARD_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/ai.h"
#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class Parameters {
    public:

    // [m x (n + 1)] matrix [+1 for bias]
    int m;
    int n;
    
    T* param_matrix;

    Parameters(int m, int n){
        this->m = m;
        this->n = n;
        this->param_matrix = new T[m*(n+1)];
    }

    int num_parameters(){
        return m*(n+1);
    }

    void init_param_matrix(const char* filepath, int offset){
        // skip the first offset entries in the file as
        // they belong to previous layer

        if constexpr (std::is_same<T, IntFp>::value){
            float* raw_params = new float[num_parameters()];
            read_next_elements(num_parameters(), raw_params, offset, filepath);
            this->param_matrix = convert_reals_to_field_rep(num_parameters(), raw_params);

            delete[] raw_params;
        } else if constexpr (std::is_same<T, float>::value) {
            read_next_elements(num_parameters(), param_matrix, offset, filepath);
        }
    }

    void read_weights_and_biases(const char* filepath, int offset){
        read_weights(filepath, offset);
        read_biases(filepath, offset + m*n);
    }

    void read_weights(const char* filepath, int offset){
        float* raw_weights = new float[m*n];
        read_next_elements(m*n, raw_weights, offset, filepath);

        if constexpr (std::is_same<T, IntFp>::value){
            IntFp* temp_weights = new IntFp[m*n];
            temp_weights = convert_reals_to_field_rep(m*n, raw_weights);

            for(int i = 0; i < m; i++){
                for(int j = 0; j < n; j++){
                    param_matrix[i*(n+1) + j] = IntFp(temp_weights[i*n + j]);
                }
            }
            delete[] temp_weights;
        } else if constexpr (std::is_same<T, float>::value) {
            for(int i = 0; i < m; i++){
                for(int j = 0; j < n; j++){
                    param_matrix[i*(n+1) + j] = (raw_weights[i*n + j]);
                }
            }
        }

        delete[] raw_weights;
    }

    void read_biases(const char* filepath, int offset){
        float* raw_biases = new float[m];
        read_next_elements(m, raw_biases, offset, filepath);

        if constexpr (std::is_same<T, IntFp>::value){
            IntFp* temp_biases = new IntFp[m];
            temp_biases = convert_reals_to_field_rep(m, raw_biases);

            for(int i = 0; i < m; i++){
                param_matrix[(i+1)*n + i] = IntFp(temp_biases[i]);
            }

            delete[] temp_biases;
        } else if constexpr (std::is_same<T, float>::value) {
            for(int i = 0; i < m; i++){
                param_matrix[(i+1)*n + i] = (raw_biases[i]);
            }
        }

        delete[] raw_biases;
    }

    void print_parameters(){
        for(int i = 0; i < m; i++){
            for(int j = 0; j < n+1; j++){
                if constexpr (std::is_same<T, IntFp>::value){
                    cout << format_EMP_IntFp(param_matrix[i*(n+1)+j], 1) << " ";
                } else if constexpr (std::is_same<T, float>::value) {
                    cout << param_matrix[i*(n+1)+j] << " ";
                }
            }
            cout << "\n";
        }
    }
};


template <typename T>
class FFLayer {
    public:
    int input_size;         // n
    int output_size;        // m

    LAYER_TYPE type;
    T* input;
    T* output;

    Parameters<T>* param;

    FFLayer(LAYER_TYPE type, int input_size, int output_size){
        if(type == INPUT && input_size != output_size){
            error("Input layer should have same input size and output size!\n");
        }

        if(type == RELU && input_size != output_size){
            error("ReLU layer should have same input size and output size!\n");
        }

        this->input_size = input_size;
        this->output_size = output_size;
        this->type = type;

        if(type == AFFINE){
            this->input = new T[input_size+1];  // +1 for bias
            this->output = new T[output_size];
            this->param = new Parameters<T>(output_size, input_size);
        }

        if(type != AFFINE){
            assert(this->input_size == this->output_size && "Input size must match output size for non-affine layers");
            this->input = new T[input_size]; 
            this->output = new T[output_size];
        }
    }

    void forward(FFLayer<T>* prev_layer){
        // clone the input
        for(int i = 0; i < input_size; i++){
            input[i] = T(prev_layer->output[i]);
        }
        if(type == AFFINE){
            if constexpr (std::is_same<T, IntFp>::value) {
                input[input_size] = IntFp(1 << FXPSCALE);
            } else if constexpr (std::is_same<T, float>::value) {
                input[input_size] = float(1);
            }
        }
        
        // forward pass
        if(type == RELU){
            relu_layer(input_size, input, output);
        }

        if(type == AFFINE){
            assert((param != NULL && param->param_matrix != NULL) && "Parameters not initialized for AFFINE layer");
            affine_layer(output_size, input_size, param->param_matrix, input, output);
            if constexpr (std::is_same<T, IntFp>::value){
                normalize(output_size, output, output);
            }
        }
    };

    void describe(bool print_parameters = true){
        cout << "Type: " << get_layer_type(type) << "\n";
        if(print_parameters){
            cout << "Parameters:\n";
            if(type == AFFINE){
                param->print_parameters();
            } else {
                cout << "NULL\n";
            }
        }

        cout << "Inputs:\n";
        for(int i = 0; i < input_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(input[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << input[i] << " ";
            }
        }
        cout << "\n";
        cout << "Outputs:\n";
        for(int i = 0; i < output_size; i++){
            if constexpr (std::is_same<T, IntFp>::value){
                cout << format_EMP_IntFp(output[i], 1) << " ";
            } else if constexpr (std::is_same<T, float>::value) {
                cout << output[i] << " ";
            }
        }
        cout << "\n\n";
    }
};

template <typename T>
class FeedForwardNeuralNetwork {
    public:

    int num_layers;
    FFLayer<T>** layers;
    TEST_MODE test_mode;
    

    FeedForwardNeuralNetwork(int num_layers, FFLayer<T>** layers){
        this->num_layers = num_layers;
        this->layers = layers;

        validate_layers();
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
            FFLayer<T>* prev_layer = layers[i-1];
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
            FFLayer<T>* curr_layer = layers[k];
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
            FFLayer<T>* curr_layer = layers[k];
            if(curr_layer->type == AFFINE){
                curr_layer->param->read_weights_and_biases(param_file_path, layer_offset);
                layer_offset += curr_layer->param->num_parameters();
            } else {
                continue;
            }
        }
    }


    void load_input(const char* input_file_path){
        assert(layers != NULL && "Layers not yet initialized, cannot load inputs!");
        assert(layers[0]->type == INPUT && "The first layer should be an INPUT layer, cannot load inputs!");

        FFLayer<T>* input_layer = layers[0];
        
        if constexpr (std::is_same<T, IntFp>::value){
            float* raw_inputs = new float[input_layer->input_size];
            read_next_elements(input_layer->input_size, raw_inputs, 0, input_file_path);
            
            input_layer->input = convert_reals_to_field_rep(input_layer->input_size, raw_inputs);
            input_layer->output = convert_reals_to_field_rep(input_layer->output_size, raw_inputs);

            delete[] raw_inputs;
        } else if constexpr (std::is_same<T, float>::value){
            read_next_elements(input_layer->input_size, input_layer->input, 0, input_file_path);        
            read_next_elements(input_layer->output_size, input_layer->output, 0, input_file_path);  
        }
    }


    void forward(){
        FFLayer<T>* prev_layer = layers[0];
        for(int i = 1; i < num_layers - 1; i++){
            // skip INPUT and OUTPUT layers
            layers[i]->forward(prev_layer);
            prev_layer = layers[i];
        }
    }

    void describe(bool print_parameters = true){
        for(int i = 0; i < num_layers; i++){
            cout << "LAYER " << (i+1) << "\n";
            layers[i]->describe(print_parameters);
        }
    }
};

#endif