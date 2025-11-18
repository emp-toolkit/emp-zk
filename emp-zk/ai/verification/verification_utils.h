#ifndef __VERIFICATION_UTILS_H__
#define __VERIFICATION_UTILS_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"

#include <fstream>

using namespace std;
using namespace emp;


enum SPEC_LABELS{
    LAYER_TYPE_INDEX,
    INPUT_SIZE_INDEX,
    OUTPUT_SIZE_INDEX,
    MAX_COEFFS_INDEX,
};

template <typename T>
VerifiableFeedForwardNeuralNetwork<T>* create_model(int num_layers, int* layer_specs){
    Layer<T>** layers = new Layer<T>*[num_layers];
    for(int i = 0; i < num_layers; i++){
        switch (layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + LAYER_TYPE_INDEX]){
            case LAYER_TYPE::INPUT:
                layers[i] = new Input<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX]
                );
                break;

            case LAYER_TYPE::AFFINE:
                layers[i] = new Affine<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX]
                );
                break;

            case LAYER_TYPE::RELU:
                layers[i] = new ReLU<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX]
                );
                break;

            case LAYER_TYPE::OUTPUT:
                layers[i] = new Output<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX]
                );
                break;

            default:
                error("Invalid Layer type!\n");
        }
    }
    VerifiableFeedForwardNeuralNetwork<T>* model = new VerifiableFeedForwardNeuralNetwork<T>(num_layers, layers);

    return model;
}


template <typename T>
bool verify_example(VerifiableFeedForwardNeuralNetwork<T>* model, const char* input_file, int input_offset, bool do_backsubstitution){
    model->reset();
    model->load_input(input_file, input_offset);
    bool verified = model->forward(do_backsubstitution, true);
    return verified;
}   


void preprocess_bounds(int n, float* bounds, float mean, float sdev){
    for(int i = 0; i < n; i++){
        bounds[i] = (bounds[i] - mean)/sdev;
    }
}


void preprocess_bounds(int dims, int n_per_dim){
    ;
}

#endif