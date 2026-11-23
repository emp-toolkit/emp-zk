#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>
using namespace emp;
using namespace std;

int party;
int index_sz = 5, val_sz = 32;

void test(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  int test_n = (1 << index_sz);   // number of cells

  // Memory content x[i] = 2*i.
  vector<ZKUInt> data;
  for (int i = 0; i < test_n; ++i)
    data.push_back(sess.input<ZKUInt>(ALICE, 2 * i, val_sz));

  int rounds = 8;
  int64_t T = (int64_t)rounds * test_n;   // number of lookups

  ZKROM *rom = new ZKROM(sess, index_sz, val_sz, T);
  rom->init(data);

  auto start = clock_start();
  // Read every cell several times.
  int wrong = 0;
  for (int r = 0; r < rounds; ++r)
    for (int i = 0; i < test_n; ++i) {
      ZKUInt res = rom->read(sess.input<ZKUInt>(PUBLIC, i, index_sz));
      ZKBit eq = res == sess.input<ZKUInt>(ALICE, i * 2, val_sz);
      if (!sess.reveal(eq, PUBLIC).value_or(false)) wrong++;
    }
  rom->check();   // proves reads ∼ writes; aborts on a forged read
  double t = time_from(start);
  expecting(wrong == 0, "rom_test: a lookup returned a wrong value");

  sess.finalize();
  int accesses = rounds * test_n;
  cout << "ROM ok (cells=" << test_n << ", accesses=" << accesses
       << ", wrong=" << wrong << ")  " << t / accesses << " us/access  party "
       << party << endl;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  test(&io, party);

  return 0;
}
