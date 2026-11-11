#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>

using namespace emp;
using namespace std;

int party;
int repeat, sz;

void test_inner_product(BoolIO *io, int party) {
  srand(1);  // both parties draw the same test values
  uint64_t constant = 0;
  uint64_t *witness = new uint64_t[2 * sz];
  memset(witness, 0, 2 * sz * sizeof(uint64_t));

  setup_zk_arith(io, party);

  IntFp *x = new IntFp[2 * sz];

  if (party == ALICE) {
    uint64_t sum = 0, tmp;
    for (int i = 0; i < sz; ++i) {
      witness[i] = rand() % PR;
      witness[sz + i] = rand() % PR;
    }
    for (int i = 0; i < sz; ++i) {
      tmp = mult_mod(witness[i], witness[sz + i]);
      sum = add_mod(sum, tmp);
    }
    constant = PR - sum;
    io->send_data(&constant, sizeof(uint64_t));
  } else {
    io->recv_data(&constant, sizeof(uint64_t));
  }

  for (int i = 0; i < 2 * sz; ++i)
    x[i] = IntFp(witness[i], ALICE);

  auto start = clock_start();
  for (int j = 0; j < repeat; ++j) {
    fp_zkp_inner_prdt(x, x + sz, constant, sz);
  }

  finalize_zk_arith();

  double tt = time_from(start);
  cout << "prove " << repeat << " degree-2 polynomial of length " << sz << endl;
  cout << "time use: " << tt / 1000 << " ms" << endl;
  cout << "average time use: " << tt / 1000 / repeat << " ms" << endl;

  delete[] witness;
  delete[] x;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  std::cout << std::endl << "------------ ";
  std::cout << "ZKP inner product test";
  std::cout << " ------------" << std::endl << std::endl;
  ;

  if (argc < 2) {
    std::cout << "usage: [binary] PARTY POLY_NUM POLY_DIMENSION"
              << std::endl;
    return -1;
  } else if (argc < 4) {
    repeat = 100;
    sz = 10;
  } else {
    repeat = atoi(argv[2]);
    sz = atoi(argv[3]);
  }

  test_inner_product(&io, party);

  return 0;
}
