#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int party;

void test_mix_circuit(BoolIO *io, int party, int sz) {
  srand(1);  // both parties draw the same test values
  uint64_t *a = new uint64_t[sz];
  for (int i = 0; i < sz; ++i)
    a[i] = rand() % PR;

  ZKBoolSession sess(io, party);
  setup_zk_arith(io, party, sess);

  IntFp *x = new IntFp[sz];
  batch_feed(x, a, sz);

  sess.flush();

  ZKInt *y = new ZKInt[sz];
  for (int i = 0; i < sz; ++i)
    y[i] = sess.input<ZKInt>(ALICE, a[i], 62);

  ZKInt PR_bl = sess.input<ZKInt>(PUBLIC, PR, 62);

  sess.flush();

  auto start = clock_start();
  for (int k = 0; k < 2; ++k) {
    for (int i = 0; i < 3; ++i) {
      for (int j = i; j < sz - 3; j += 3) {
        y[j + 2] = y[j + 1] + y[j];
        y[j + 2] = y[j + 2].select(y[j + 2][61], y[j + 2] - PR_bl);

        a[j + 2] = a[j + 1] + a[j];
        if (a[j + 2] > PR)
          a[j + 2] -= PR;
      }
      bool2arith(x, y, sz);

      for (int j = i; j < sz - 3; j += 3) {
        x[j] = x[j + 1] + x[j + 2];
        a[j] = a[j + 1] + a[j + 2];
        if (a[j] > PR)
          a[j] -= PR;
      }
      arith2bool(y, x, sz);
    }
  }

  int incorrect_cnt = 0;
  for (int i = 0; i < sz; ++i) {
    ZKBit ret = (y[i] == sess.input<ZKInt>(PUBLIC, a[i], 62));
    if (sess.reveal(ret, PUBLIC).value_or(false) != 1)
      incorrect_cnt++;
  }
  if (incorrect_cnt)
    std::cout << "incorrect boolean: " << incorrect_cnt << std::endl;
  std::cout << "end check boolean" << std::endl;

  batch_reveal_check(x, a, sz);
  std::cout << "end check arithmetic" << std::endl;

  sess.finalize();
  finalize_zk_arith();

  double tt = time_from(start);
  std::cout << "conversion: " << tt / (2 * 3 * 2) / sz << std::endl;

  delete[] a;
  delete[] x;
  delete[] y;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  std::cout << std::endl
            << "------------ circuit zero-knowledge proof test ------------"
            << std::endl
            << std::endl;
  ;

  int num = 0;
  if (argc < 2) {
    std::cout << "usage: bin/arith/abconversion PARTY TEST_SIZE"
              << std::endl;
    return -1;
  } else if (argc == 2) {
    num = 10;
  } else {
    num = atoi(argv[2]);
  }

  test_mix_circuit(&io, party, num);

  return 0;
}
