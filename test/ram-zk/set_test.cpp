#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>
using namespace emp;
using namespace std;

int party;

// ZKSet is the freshness primitive of the read/write RAM: the public range
// {1, …, T}, with prove_member(v) proving v is in range. An in-range stream
// must pass; test/ram-zk/cheat.cpp checks that queries of a non-member fail.
void test(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  int64_t T = 64;
  int elem_sz = (int)ramzk_bits_for(T);

  ZKSet *s = new ZKSet(sess, T, elem_sz, 3 * T);   // 3T queries below

  // Query each element of {1..T}, several elements repeatedly (chains).
  for (int rep = 0; rep < 3; ++rep)
    for (int64_t e = 1; e <= T; ++e)
      s->prove_member(sess.input<ZKUInt>(ALICE, (uint64_t)e, elem_sz));

  s->check();
  sess.finalize();
  cout << "ZKSet ok (T=" << T << ")  party " << party << endl;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  test(&io, party);

  return 0;
}
