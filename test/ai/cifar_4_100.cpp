#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <emp-zk/ai/ai.h>

#include <iostream>
#include <fstream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int num_examples = 1;
float epsilon = 0.1;
int num_neurons = 100;

const char* INPUTS_PATH = "test/ai/data/inputs/cifar_relu.txt";
const char* PARAMETERS_PATH = "test/ai/data/parameters/cifar_relu_7_1024.txt";
const char* LOGS_PATH = "test/ai/data/logs/cifar_7_1024";


void test_verification(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);


  int layer_specs[] = {
    INPUT, 1024, 1024, -1,

    AFFINE, 1024, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, 10, -1,
    RELU, 10, 10, -1,

    OUTPUT, 10, 10, -1
  };


  // float 
  std::ofstream file(std::string(LOGS_PATH) + "_float.txt");
  std::streambuf* original_buf = std::cout.rdbuf(file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<float>* model_float = create_model<float>(16, layer_specs);
  model_float->load_input(INPUTS_PATH);
  model_float->load_weights_and_biases(PARAMETERS_PATH);
  model_float->set_epsilon(epsilon);

  int num_examples_verified = 0;
  for(int i = 0; i < num_examples; i++){
    bool verified = model_float->forward(true);
    num_examples_verified += (int) verified;
  }
  cout << "Verified " << num_examples_verified << "/" << num_examples << " examples\n";

  model_float->describe(false);

  
  // field
  std::cout.rdbuf(original_buf);  
  std::ofstream field_file(std::string(LOGS_PATH) + ".txt");
  original_buf = std::cout.rdbuf(field_file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<IntFp>* model_field = create_model<IntFp>(16, layer_specs);
  model_field->load_input(INPUTS_PATH);
  model_field->load_weights_and_biases(PARAMETERS_PATH);
  model_field->set_epsilon(epsilon);

  num_examples_verified = 0;
  for(int i = 0; i < num_examples; i++){
    bool verified = model_field->forward(true);
    num_examples_verified += (int) verified;
  }
  cout << "Verified " << num_examples_verified << "/" << num_examples << " examples\n";

  model_field->describe(false);

  std::cout.rdbuf(original_buf);
}

int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);

  if(argc > 3){
    num_examples = atoi(argv[3]);
  }

  if(argc > 4){
    epsilon = (atoi(argv[4])*1.0)/1000;
  }

  if(argc > 5){
    num_neurons = atoi(argv[5]);
  }


  test_verification(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }

  return 0;
}
