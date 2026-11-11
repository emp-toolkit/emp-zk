#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>
using namespace emp;
using namespace std;

int party;
int repeat, sz;

void test_inner_product(BoolIO *io, int party) {
  srand(1);  // both parties draw the same test values
  bool constant;
  bool *witness = new bool[2 * sz];
  memset(witness, 0, 2 * sz * sizeof(bool));

  ZKBoolSession sess(io, party);
  sess.flush();

  ZKBit *x = new ZKBit[2 * sz];

  if (party == ALICE) {
    bool sum = 0, tmp;
    PRG prg;
    prg.random_bool(witness, 2 * sz);
    for (int i = 0; i < sz; ++i) {
      tmp = witness[i] & witness[sz + i];
      sum = sum ^ tmp;
    }
    constant = sum;
    io->send_data(&constant, sizeof(bool));
  } else {
    io->recv_data(&constant, sizeof(bool));
  }
  io->flush();

  for (int i = 0; i < 2 * sz; ++i)
    x[i] = sess.input<ZKBit>(ALICE, witness[i]);

  sess.flush();
  auto start = clock_start();
  for (int j = 0; j < repeat; ++j) {
    zkp_inner_prdt(sess, x, x + sz, constant, sz);
  }

  sess.finalize();

  double tt = time_from(start);
  cout << "prove " << repeat << " degree-2 inner_product of length " << sz
       << endl;
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
  std::cout << "ZKP inner_product test";
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
