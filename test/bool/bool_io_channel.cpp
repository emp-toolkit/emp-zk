// BoolIO as the channel of a proof: it uses its connection's Fiat-Shamir
// transcript, and it hands the extensions it carries their session ids from
// that connection. Checks that (1) a byte message that closes a partial chunk
// of bits leaves the two parties' transcripts in agreement, and BoolIO's digest
// is its channel's; (2) BoolIO reports its channel's session root, and two
// proofs over one BoolIO give their Ferrets fresh ids that both parties share.

#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>
using namespace emp;
using namespace std;

using Int32 = Int_T<ZKBoolSession::ctx_t, 32>;

// Send a block to the peer over the connection and return the peer's.
static block exchange(NetIO *netio, int party, block mine) {
  block theirs;
  if (party == ALICE) {
    netio->send_block(&mine, 1);
    netio->recv_block(&theirs, 1);
  } else {
    netio->recv_block(&theirs, 1);
    netio->send_block(&mine, 1);
    netio->flush();
  }
  return theirs;
}

static void check_one_transcript(NetIO *netio, BoolIO *io, int party) {
  const int nbits = 1000;  // less than a chunk
  const block msg = makeBlock(0x1234, 0x5678);
  if (party == ALICE) {
    for (int i = 0; i < nbits; ++i) io->send_bit(i % 3 == 0);
    io->send_block(&msg, 1);  // closes the partial chunk
  } else {
    for (int i = 0; i < nbits; ++i)
      if (io->recv_bit() != (i % 3 == 0)) error("BoolIO: a bit arrived wrong");
    block got;
    io->recv_block(&got, 1);
    if (!cmpBlock(&got, &msg, 1)) error("BoolIO: the message arrived wrong");
  }
  io->flush();
  const block mine = io->get_digest();
  const block inner = netio->get_digest();
  if (!cmpBlock(&mine, &inner, 1)) error("BoolIO: its digest is not its channel's");
  const block theirs = exchange(netio, party, mine);
  if (!cmpBlock(&mine, &theirs, 1)) error("BoolIO: the parties' transcripts differ");
  cout << "one transcript, in wire order: OK" << endl;
}

// Run a small proof over `io` and return the session id its Ferret took.
static block one_proof_sid(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  Int32 a = sess.input<Int32>(ALICE, 3);
  Int32 b = sess.input<Int32>(ALICE, 2);
  if (sess.reveal(a - b, PUBLIC).value_or(-1) != 1) error("the proof revealed a wrong value");
  const block sid = sess.engine().ferret->session_id().value();
  sess.finalize();
  return sid;
}

static void check_session_ids(NetIO *netio, BoolIO *io, int party) {
  if (!io->has_session_root()) error("BoolIO: no session root over a NetIO");
  const block s1 = one_proof_sid(io, party);
  const block s2 = one_proof_sid(io, party);
  if (cmpBlock(&s1, &zero_block, 1) || cmpBlock(&s2, &zero_block, 1))
    error("a proof's Ferret kept the zero session id");
  if (cmpBlock(&s1, &s2, 1)) error("two proofs got the same session id");
  io->flush();
  const block t1 = exchange(netio, party, s1);
  const block t2 = exchange(netio, party, s2);
  if (!cmpBlock(&s1, &t1, 1) || !cmpBlock(&s2, &t2, 1))
    error("the parties' session ids differ");
  cout << "fresh session ids from the channel: OK" << endl;
}

int main(int argc, char **argv) {
  int party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port())
                                : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);
  check_one_transcript(netio.get(), &io, party);
  check_session_ids(netio.get(), &io, party);
  return 0;
}
