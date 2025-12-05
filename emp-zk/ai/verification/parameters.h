#ifndef __PARAMETERS_H__
#define __PARAMETERS_H__

#pragma once

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/ai/ai.h"
#include "emp-zk/ai/secure-utils.h"
#include <iostream>

using namespace emp;
using namespace std;

template <typename T>
class Parameters {
    public:

    // [m x (n + 1)] matrix [+1 for bias]
    int m;
    int n;
    int party = PUBLIC;

    T* param_matrix;

    Parameters(int m, int n, int party = PUBLIC){
        this->m = m;
        this->n = n;
        this->party = party;
        this->param_matrix = new T[m*(n+1)];
    }

    int num_parameters(){
        return m*(n+1);
    }


    void read_weights_and_biases(const char* filepath, int offset){
        read_weights(filepath, offset);
        read_biases(filepath, offset + m*n);
    }

    void read_weights(const char* filepath, int offset){
        float* raw_weights = new float[m*n];
        if(this->party != BOB){
            read_next_elements(m*n, raw_weights, offset, filepath);
        }

        if constexpr (std::is_same<T, IntFp>::value){
            IntFp* temp_weights = new IntFp[m*n];
            authenticate_over_field(m*n, raw_weights, temp_weights, this->party);
            
            for(int i = 0; i < m; i++){
                for(int j = 0; j < n; j++){
                    param_matrix[i*(n+1) + j] = temp_weights[i*n + j];
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

        if(this->party != BOB){
            read_next_elements(m, raw_biases, offset, filepath);
        }

        if constexpr (std::is_same<T, IntFp>::value){
            IntFp* temp_biases = new IntFp[m];
            authenticate_over_field(m, raw_biases, temp_biases, this->party);

            for(int i = 0; i < m; i++){
                param_matrix[(i+1)*n + i] = temp_biases[i];
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