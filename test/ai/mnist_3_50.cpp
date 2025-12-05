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
int base_example = 0;

const char* INPUTS_PATH = "test/ai/data/inputs/mnist_test.txt";
const char* PARAMETERS_PATH = "test/ai/data/parameters/mnist_relu_3_50.txt";
const char* LOGS_PATH = "test/ai/data/logs/mnist_3_50/mnist_3_50";


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

void test_verification(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

  init_verification();
  startComputation(party);

  int num_layers = (sizeof(layer_specs)/sizeof(int))/4;

  auto start = clock_start();

  int num_examples_verified = 0;

  // float 
  std::ofstream file(std::string(LOGS_PATH) + "_float_worker_" + std::to_string(port - 10000 + 1) + ".txt");
  std::streambuf* original_buf = std::cout.rdbuf(file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<float>* model_float = create_model<float>(num_layers, layer_specs, party);
  model_float->load_input(INPUTS_PATH, 0, epsilon);
  model_float->load_weights_and_biases(PARAMETERS_PATH);

  for(int i = base_example; i < base_example + num_examples; i++){
    cout << "EXAMPLE " << i + 1 << ":\n";
    bool verified = verify_example<float>(model_float, INPUTS_PATH, i*785, epsilon);
    num_examples_verified += (int) verified;
  }
  cout << "Verified " << num_examples_verified << "/" << num_examples << " examples\n";

  // model_float->describe(false, true);

  std::cout.rdbuf(original_buf);  

  
  // field
  std::ofstream field_file(std::string(LOGS_PATH) + "_worker_" + std::to_string(port - 10000 + 1) + ".txt");
  std::streambuf* original_buf2 = std::cout.rdbuf(field_file.rdbuf());

  VerifiableFeedForwardNeuralNetwork<IntFp>* model_field = create_model<IntFp>(num_layers, layer_specs, party);
  model_field->load_input(INPUTS_PATH, epsilon);
  model_field->load_weights_and_biases(PARAMETERS_PATH);

  num_examples_verified = 0;
  for(int i = base_example; i < base_example + num_examples; i++){
    cout << "EXAMPLE " << i + 1 << ":\n";
    bool verified = verify_example<IntFp>(model_field, INPUTS_PATH, i*785, epsilon);
    num_examples_verified += (int) verified;
    flush(cout);
  }

  // model_field->describe(false, true);

  std::cout.rdbuf(original_buf2);

  bool cheated = finalize_zk_arith<BoolIO<NetIO>>();
  if(party == BOB){
    cout << "\n" << (cheated ? "\033[31mVerfication failed!" : "\033[32mVerfication successful!") << "\033[0m\n";
  }

  double tt = time_from(start);
  cout << "\nAvg. time to verify: " << (tt/1000000)/num_examples << " s\n";
  cout << "Avg. Communication: " << (ios[0]->counter/(1024.0 * 1024.0))/num_examples << " MB\n";
}

int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);

  if(argc > 3){
    base_example = atoi(argv[3]);
  }

  if(argc > 4){
    epsilon = (atoi(argv[4])*1.0)/1000;
  }

  if(argc > 5){
    num_examples = atoi(argv[5]);
  }

  // fflush(stdout);
  test_verification(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }

  return 0;
}
