#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include <iostream>
using namespace emp;
using namespace std;

int party;

void test_circuit_zk(BoolIO *io, int party) {
  int test_n = 1024 * 64;

  std::cout << "performance test" << std::endl;
  auto start = clock_start();
  setup_zk_arith(io, party);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup * 1000 << " " << party << " " << endl;

  __uint128_t ar = 2, br = 3, cr = 4;
  IntFp a((uint64_t)ar, ALICE);
  IntFp b((uint64_t)br, ALICE);
  IntFp c((uint64_t)cr, PUBLIC);

  for (int i = 0; i < test_n; ++i) {
    br = (br + ar) % pr;
    ar = (br * ar) % pr;
  }
  cr = (ar * br) % pr;
  cr = (cr + ar) % pr;

  start = clock_start();
  for (int i = 0; i < test_n; ++i) {
    b = b + a;
    a = b * a;
  }
  c = a * b;
  c = c + a;

  c.reveal(cr);
  auto timeuse = time_from(start);
  cout << "\taverage time per gate: " << (timeuse + timesetup) / test_n * 1000
       << " ns " << party << endl;

  std::cout << "correctness check test" << std::endl;
  srand(1);  // both parties draw the same test values
  int sz = 10000;
  uint64_t *d = new uint64_t[sz];
  uint64_t *e = new uint64_t[sz];
  IntFp *di = new IntFp[sz];
  IntFp *ei = new IntFp[sz];
  IntFp *fi = new IntFp[sz];
  IntFp PRi = IntFp(PR - 1, PUBLIC);
  for (int i = 0; i < sz; ++i) {
    d[i] = rand() % PR;
    e[i] = rand() % PR;

    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);

    d[i] = (d[i] * e[i]) % PR;
    di[i] = di[i] * ei[i];

    e[i] = (d[i] + e[i]) % PR;
    ei[i] = di[i] + ei[i];

    fi[i] = IntFp(e[i], ALICE);
    fi[i] = PRi * fi[i];
    fi[i] = ei[i] + fi[i];
  }
  std::cout << "\treveal" << std::endl;
  uint64_t *dd = new uint64_t[sz];
  batch_reveal(di, dd, sz);
  if (memcmp(dd, d, sz * sizeof(uint64_t)) != 0)
    error("reveal fails");
  std::cout << "\treveal check" << std::endl;
  batch_reveal_check(ei, e, sz);
  std::cout << "\treveal check zero" << std::endl;
  batch_reveal_check_zero(fi, sz);

  std::cout << std::endl;

  finalize_zk_arith();

  delete[] d;
  delete[] e;
  delete[] di;
  delete[] fi;
  delete[] ei;
  delete[] dd;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  std::cout << std::endl
            << "------------ circuit zero-knowledge proof test ------------"
            << std::endl
            << std::endl;

  test_circuit_zk(&io, party);

  return 0;
}
