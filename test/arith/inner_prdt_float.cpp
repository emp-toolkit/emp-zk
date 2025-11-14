#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
int repeat, sz;
const int threads = 1;

void test_inner_product(BoolIO<NetIO> *ios[threads], int party) {
  srand(time(NULL));
  float constant = 0;
  float *witness = new float[2 * sz];
  memset(witness, 0, 2 * sz * sizeof(float));

  setup_plain_prot(false, "");
  // setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

  Float *x = new Float[2 * sz];

  ios[0]->counter = 0;
  if (party == ALICE) {
    float sum = 0, tmp;
    for (int i = 0; i < sz; ++i) {
      witness[i] = 3.141;
      witness[sz + i] = 1.893;
    }
    for (int i = 0; i < sz; ++i) {
      tmp = (witness[i] * witness[sz + i]);
      sum = (sum + tmp);
    }
    constant = sum;
    ios[0]->send_data(&constant, sizeof(float));
  } else {
    ios[0]->recv_data(&constant, sizeof(float));
  }

  for (int i = 0; i < 2 * sz; ++i)
    x[i] = Float(witness[i], ALICE);

  auto start = clock_start();
  for (int j = 0; j < repeat; ++j) {
    Float sum(0, ALICE); 
    Float tmp(0, ALICE);
    for (int i = 0; i < sz; ++i) {
      tmp = (witness[i] * witness[sz + i]);
      sum = (sum + tmp);
    }
  }

  // finalize_zk_arith<BoolIO<NetIO>>();
  finalize_plain_prot();

  double tt = time_from(start);
  cout << "prove " << repeat << " degree-2 polynomial of length " << sz << endl;
  cout << "time use: " << tt / 1000 << " ms" << endl;
  cout << "average time use: " << tt / 1000 / repeat << " ms" << endl;
  cout << "Comm: " << ios[0]->counter/sz << " bytes \n";

  delete[] witness;
  delete[] x;
}

int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);

  std::cout << std::endl << "------------ ";
  std::cout << "ZKP inner product test";
  std::cout << " ------------" << std::endl << std::endl;
  ;

  if (argc < 3) {
    std::cout << "usage: [binary] PARTY PORT POLY_NUM POLY_DIMENSION"
              << std::endl;
    return -1;
  } else if (argc < 5) {
    repeat = 100;
    sz = 10;
  } else {
    repeat = atoi(argv[3]);
    sz = atoi(argv[4]);
  }

  test_inner_product(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
