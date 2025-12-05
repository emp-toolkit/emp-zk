#ifndef __VERIFICATION_UTILS_H__
#define __VERIFICATION_UTILS_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/verification/verification.h"
#include "emp-zk/ai/json.hpp"

#include <fstream>
#include <filesystem>
#include <sys/stat.h>
#include <sys/types.h>

using namespace std;
using namespace emp;
using json = nlohmann::json;


enum SPEC_LABELS{
    LAYER_TYPE_INDEX,
    INPUT_SIZE_INDEX,
    OUTPUT_SIZE_INDEX,
    MAX_COEFFS_INDEX,
};

template <typename T>
VerifiableFeedForwardNeuralNetwork<T>* create_model(int num_layers, int* layer_specs, int party){
    Layer<T>** layers = new Layer<T>*[num_layers];
    for(int i = 0; i < num_layers; i++){
        switch (layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + LAYER_TYPE_INDEX]){
            case LAYER_TYPE::INPUT:
                layers[i] = new Input<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX],
                    party
                );
                break;

            case LAYER_TYPE::AFFINE:
                layers[i] = new Affine<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX],
                    party
                );
                break;

            case LAYER_TYPE::RELU:
                layers[i] = new ReLU<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX],
                    party
                );
                break;

            case LAYER_TYPE::OUTPUT:
                layers[i] = new Output<T>(
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + INPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + OUTPUT_SIZE_INDEX],
                    layer_specs[i*(SPEC_LABELS::MAX_COEFFS_INDEX+1) + MAX_COEFFS_INDEX],
                    party
                );
                break;

            default:
                error("Invalid Layer type!\n");
        }
    }
    VerifiableFeedForwardNeuralNetwork<T>* model = new VerifiableFeedForwardNeuralNetwork<T>(num_layers, layers, party);

    return model;
}

LAYER_TYPE stringToLayerType(const std::string& type) {
    if (type == "INPUT") return INPUT;
    if (type == "AFFINE") return AFFINE;
    if (type == "RELU") return RELU;
    if (type == "OUTPUT") return OUTPUT;
    throw std::runtime_error("Unknown layer type: " + type);
}

vector<int> read_exp_specs(
    const char* config_file_path,
    float* epsilon,
    string &INPUT_FILE_PATH,
    string &PARAMS_FILE_PATH,
    string &LOG_FILE_PATH,
    int* test_mode,
    int worker_id
){
    // Read JSON file
    std::ifstream file(config_file_path);
    if (!file.is_open()) {
        std::cerr << "Failed to open" << config_file_path << std::endl;
        return {};
    }
    
    json config;
    file >> config;
    file.close();

    string model_name = config["model_name"];
    if(model_name.compare(0, 5, "mnist") == 0){
        CURR_DATASET = DATASETS::MNIST;
        INPUT_FILE_PATH = "test/ai/data/inputs/mnist_test_" + to_string(worker_id) + ".txt";

    } else if(model_name.compare(0, 5, "cifar") == 0) {
        CURR_DATASET = DATASETS::CIFAR10;
        INPUT_FILE_PATH = "test/ai/data/inputs/cifar10_test_nonconv_" + to_string(worker_id) + ".txt";

    }

    int num_neurons = config["num_neurons"];
    std::vector<int> layer_specs;

    for (const auto& layer : config["layers"]) {
        std::string type = layer[0];
        layer_specs.push_back(stringToLayerType(type));
        
        // Handle input_size
        if (layer[1].is_string() && layer[1] == "num_neurons") {
            layer_specs.push_back(num_neurons);
        } else {
            layer_specs.push_back(layer[1]);
        }
        
        // Handle output_size
        if (layer[2].is_string() && layer[2] == "num_neurons") {
            layer_specs.push_back(num_neurons);
        } else {
            layer_specs.push_back(layer[2]);
        }
        
        layer_specs.push_back(layer[3]);
    }

    *epsilon = (float) config["epsilon"];


    PARAMS_FILE_PATH = "test/ai/data/parameters/" + model_name + "_" + to_string(worker_id) + ".txt";


    string folder = "test/ai/data/logs/" + model_name;
    struct stat st;
    if (stat(folder.c_str(), &st)) {
        mkdir(folder.c_str(), 0755) == 0;
    }
    LOG_FILE_PATH = folder + "/" + model_name;
    
    *test_mode = (int) config["do_float"];

    FXPSCALE = (int) config["FXPSCALE"];
    INPUT_MIN = (float) config["input_min"];
    INPUT_MAX = (float) config["input_max"];

    return layer_specs;
}


template <typename T>
bool verify_example(VerifiableFeedForwardNeuralNetwork<T>* model, const char* input_file, int input_offset, float epsilon){
    model->reset();
    model->load_input(input_file, input_offset, epsilon);
    bool verified = model->forward(true, true);
    return verified;
}   


void preprocess_input(int n, float* inputs, float mean, float sdev){
    for(int i = 0; i < n; i++){
        inputs[i] = (inputs[i] - mean)/sdev;
    }
}


void preprocess_input(int dims, int n_per_dim){
    ;
}

#endif