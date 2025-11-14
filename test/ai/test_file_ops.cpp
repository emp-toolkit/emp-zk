#include <iostream>
#include <fstream>
#include <vector>
#include <type_traits>

#include "emp-zk/ai/ai.h"

using namespace std;

int main() {
    try {
        float* real_values = new float[3];
        read_next_elements(1, real_values, 1, "test_path.txt");
        for (int i = 0; i < 3; i++){
            auto v = real_values[i];
            std::cout << std::setprecision(10) << v << " ";
        }
        std::cout << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}