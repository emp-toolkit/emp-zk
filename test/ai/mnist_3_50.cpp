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
int num_neurons = 50;
bool do_backsubstitution = false;

const char* INPUTS_PATH = "test/ai/data/inputs/mnist_test.txt";
const char* PARAMETERS_PATH = "test/ai/data/parameters/mnist_relu_3_50.txt";
const char* LOGS_PATH = "test/ai/data/logs/mnist_3_50";


void test_verification(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);


  int layer_specs[] = {
    INPUT, 784, 784, -1,

    AFFINE, 784, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, num_neurons, -1,
    RELU, num_neurons, num_neurons, -1,

    AFFINE, num_neurons, 10, -1,
    RELU, 10, 10, -1,

    OUTPUT, 10, 10, -1
  };
  int num_layers = (sizeof(layer_specs)/sizeof(int))/4;


  // float 
  std::ofstream file(std::string(LOGS_PATH) + (do_backsubstitution ? "_float_bs.txt" : "_float.txt"));
  std::streambuf* original_buf = std::cout.rdbuf(file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<float>* model_float = create_model<float>(num_layers, layer_specs);
  model_float->load_input(INPUTS_PATH);
  model_float->load_weights_and_biases(PARAMETERS_PATH);
  model_float->set_epsilon(epsilon);

  int num_examples_verified = 0;
  for(int i = num_examples-1; i < num_examples; i++){
    // bool verified = model_float->forward(true);
    bool verified = verify_example<float>(model_float, INPUTS_PATH, 1 + i*785, do_backsubstitution);
    num_examples_verified += (int) verified;
  }
  cout << "Verified " << num_examples_verified << "/" << num_examples << " examples\n";

  model_float->describe(false);

  std::cout.rdbuf(original_buf);  
  cout << "Float completed...\n";

  /*
  // field
  std::ofstream field_file(std::string(LOGS_PATH) + (do_backsubstitution ? "_bs.txt" : ".txt"));
  std::streambuf* original_buf2 = std::cout.rdbuf(field_file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<IntFp>* model_field = create_model<IntFp>(num_layers, layer_specs);
  model_field->load_input(INPUTS_PATH);
  model_field->load_weights_and_biases(PARAMETERS_PATH);
  model_field->set_epsilon(epsilon);

  num_examples_verified = 0;
  for(int i = 0; i < num_examples; i++){
    // bool verified = model_field->forward(true);
    bool verified = verify_example<IntFp>(model_field, INPUTS_PATH, 1 + i*785, false);
    num_examples_verified += (int) verified;
  }
  cout << "Verified " << num_examples_verified << "/" << num_examples << " examples\n";

  model_field->describe(false);

  std::cout.rdbuf(original_buf2);
  */
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
    do_backsubstitution = (bool) atoi(argv[5]);
  }


  test_verification(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }

  return 0;
}
