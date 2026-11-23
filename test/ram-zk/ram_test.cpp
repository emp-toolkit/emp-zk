#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>
using namespace emp;
using namespace std;

int party;
// val_sz is wide on purpose: index_sz + val_sz + time_sz > 128 forces each
// record to span two f2k wires, exercising ZKPermProof's multi-block compress
// path. Values themselves stay < 2^16. Single-block records are covered by
// rom_test/set_test.
int index_sz = 5, val_sz = 130;

// Two-shuffles read/write RAM (unified ZKRam, read/write mode): init, an
// interleaved load/store stream that mutates cells, read-back checks, then
// check(). test/ram-zk/cheat.cpp runs the cheating provers.
void test(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  int cells = (1 << index_sz);

  int rounds = 4;
  int64_t T = (int64_t)rounds * cells * 2;   // each sweep: 1 write + 1 read / cell

  vector<ZKUInt> data;
  for (int i = 0; i < cells; ++i)
    data.push_back(sess.input<ZKUInt>(ALICE, i, val_sz));

  ZKRam *ram = new ZKRam(sess, index_sz, val_sz, T);
  ram->init(data);

  auto start = clock_start();
  int wrong = 0;
  vector<uint64_t> expect(cells);
  for (int i = 0; i < cells; ++i)
    expect[i] = i;

  for (int r = 0; r < rounds; ++r)
    for (int i = 0; i < cells; ++i) {
      expect[i] += cells;
      ram->write(sess.input<ZKUInt>(PUBLIC, i, index_sz),
                 sess.input<ZKUInt>(ALICE, expect[i], val_sz));
      ZKUInt got = ram->read(sess.input<ZKUInt>(PUBLIC, i, index_sz));
      ZKBit eq = got == sess.input<ZKUInt>(ALICE, expect[i], val_sz);
      if (!sess.reveal(eq, PUBLIC).value_or(false))
        wrong++;
    }
  ram->check();   // both shuffles; aborts on a forged value
  double t = time_from(start);
  expecting(wrong == 0, "ram_test: a load returned a wrong value");

  delete ram;
  sess.finalize();
  int accesses = rounds * cells * 2;
  cout << "RAM ok (cells=" << cells << ", accesses=" << accesses
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
