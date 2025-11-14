#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

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

#endif