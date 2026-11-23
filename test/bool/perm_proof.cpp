#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>
#include <string>
using namespace emp;
using namespace std;

int party;
const int W = 64;   // bit width of each fed value

// Build a valid authenticated f2k wire from a fresh ALICE-input value: feed W
// authenticated bits and pack them into one F(2^128) element via the local
// Σ·Xⁱ map (mac from the bit wires, val from the cleartext LSBs).
static F2kAuthValue make_wire(ZKBoolSession &sess, uint64_t v) {
  ZKInt x = sess.input<ZKInt>(ALICE, v, W);
  vector<F2kAuthValue> out;
  ramzk_pack_record(&sess.engine(), {&x}, out);
  return out[0];
}

// n elements, each `m` wires (payload = m*128 bits). A is the natural order;
// B is the same elements in reversed order (a permutation).
// test/ram-zk/cheat.cpp checks that a multiset that differs fails.
void test_perm(BoolIO *io, int party, int n, int bits,
               const string &mode) {
  ZKBoolSession sess(io, party);
  const int m = (bits + 127) / 128;
  // "periodic" exercises explicit per-batch compress(): each fold draws its
  // own coefficient, so matched elements must stay in the same batch — use
  // identity order. Otherwise B is the reversed permutation, folded once.
  const bool periodic = (mode == "periodic");

  vector<F2kAuthValue> elems((size_t)n * m);
  for (int i = 0; i < n; ++i)
    for (int j = 0; j < m; ++j)
      elems[(size_t)i * m + j] =
          make_wire(sess, ((uint64_t)i * 2654435761ull + 7) ^ ((uint64_t)j << 40));

  auto start = clock_start();
  ZKPermProof perm(sess, bits);
  const int chunk = (n >= 4) ? n / 4 : n;
  for (int i = 0; i < n; ++i) {
    perm.add_A(&elems[(size_t)i * m]);
    int src = periodic ? i : (n - 1 - i);
    perm.add_B(&elems[(size_t)src * m]);
    if (periodic && (i + 1) % chunk == 0) perm.compress();   // fold this batch
  }
  perm.check_eq();   // passes iff B is a permutation of A
  double t = time_from(start);

  sess.finalize();
  cout << "perm check (n=" << n << ", bits=" << bits << " [" << m << " blk]"
       << (mode.empty() ? "" : ", " + mode) << ") PASSED\t" << t
       << " us\tparty " << party << endl;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  int n = (argc >= 3) ? atoi(argv[2]) : 100000;
  int bits = (argc >= 4) ? atoi(argv[3]) : 128;
  string mode = (argc >= 5) ? string(argv[4]) : "";   // "" or "periodic"
  test_perm(&io, party, n, bits, mode);

  return 0;
}
