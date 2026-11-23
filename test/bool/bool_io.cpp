#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>
using namespace emp;
using namespace std;
// Whole chunks of bits plus a partial last one.
const int LL = 1024 * 1024 * 4 + 10;
int main(int argc, char **argv) {
  int party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port(), false)
                                : NetIO::connect(peer_ip(), peer_port(), false);
  BoolIO io(netio.get(), party == ALICE);
  bool *data = new bool[LL];
  bool *data2 = new bool[LL];

  // Agree on a fresh PRG seed so BOB can re-derive the expected bit
  // stream. Done over netio before the timed bit-streaming loop so
  // the round-trip stays out of the throughput measurement.
  block test_seed;
  if (party == ALICE) {
    PRG().random_block(&test_seed, 1);
    netio->send_data(&test_seed, sizeof(block));
  } else {
    netio->recv_data(&test_seed, sizeof(block));
  }
  netio->flush();

  PRG prg(&test_seed);
  prg.random_bool(data, LL);
  auto t1 = clock_start();
  if (party == ALICE) {
    for (int i = 0; i < LL; ++i)
      io.send_bit(data[i]);
  } else {
    for (int i = 0; i < LL; ++i)
      data[i] = io.recv_bit();
  }
  io.flush();
  cout << time_from(t1) / LL * 1000 << "\n";
  if (party == BOB) {
    PRG prg2(&test_seed);
    prg2.random_bool(data2, LL);
    expecting(memcmp(data, data2, LL) == 0,
              "BoolIO: the received bits differ from the sent ones");
    cout << "fine!\n";
  }
  return 0;
}
