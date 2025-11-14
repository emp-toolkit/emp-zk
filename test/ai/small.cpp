#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <emp-zk/ai/ai.h>

#include <iostream>
#include <fstream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

const char* INPUTS_PATH = "test/ai/data/inputs/small.txt";
const char* PARAMETERS_PATH = "test/ai/data/parameters/small.txt";
const char* LOGS_PATH = "test/ai/data/logs/small";

void test_small_model(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

  FFLayer<IntFp>* layers[] = {
    new FFLayer<IntFp>(LAYER_TYPE::INPUT, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::AFFINE, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::RELU, 1024, 1024),
    new FFLayer<IntFp>(LAYER_TYPE::OUTPUT, 1024, 1024),
  };
  FFLayer<IntFp>** net_layers = layers;
  FeedForwardNeuralNetwork<IntFp> small(sizeof(layers)/sizeof(layers[0]), net_layers);

  small.load_input(INPUTS_PATH);
  // small.load_network_parameters(PARAMETERS_PATH);
  small.load_weights_and_biases(PARAMETERS_PATH);

  small.forward();

  small.describe(false);
}


void test_small_verification(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

  std::ofstream file(std::string(LOGS_PATH) + "_float.txt");
  std::streambuf* original_buf = std::cout.rdbuf(file.rdbuf());  

  // float 
  Layer<float>* layers[] = {
    new Input<float>(10, 10, 1, 0.1),
    new Affine<float>(10, 10, 11),
    new ReLU<float>(10, 10, 2),
    new Affine<float>(10, 10, 11),
    new ReLU<float>(10, 10, 2),
    new Output<float>(10, 10, 2)
  };
  Layer<float>** net_layers = layers;
  VerifiableFeedForwardNeuralNetwork<float> small_float(sizeof(layers)/sizeof(layers[0]), net_layers);

  small_float.load_input(INPUTS_PATH);
  small_float.load_weights_and_biases(PARAMETERS_PATH);

  bool verified = small_float.forward(true);

  small_float.describe(false);

  

  std::cout.rdbuf(original_buf);  
  std::ofstream field_file(std::string(LOGS_PATH) + ".txt");
  original_buf = std::cout.rdbuf(field_file.rdbuf());  

  // field
  Layer<IntFp>* layers_field[] = {
    new Input<IntFp>(10, 10, 1, 0.1),
    new Affine<IntFp>(10, 10, 11),
    new ReLU<IntFp>(10, 10, 2),
    new Affine<IntFp>(10, 10, 11),
    new ReLU<IntFp>(10, 10, 2),
    new Output<IntFp>(10, 10, 2)
  };
  Layer<IntFp>** net_layers_field = layers_field;
  VerifiableFeedForwardNeuralNetwork<IntFp> small_field(sizeof(layers_field)/sizeof(layers_field[0]), net_layers_field);

  small_field.load_input(INPUTS_PATH);
  small_field.load_weights_and_biases(PARAMETERS_PATH);

  verified = small_field.forward(true);

  small_field.describe(false);

  std::cout.rdbuf(original_buf);
}

int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);

  // test_small_model(ios, party);
  test_small_verification(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }

  return 0;
}
