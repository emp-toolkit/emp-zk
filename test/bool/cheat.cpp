// The Boolean checks against a cheating prover, and the honest runs at their
// batch boundaries. Each deviation is the simplest one a check must reject: a
// prover that commits one wrong value under a consistent MAC, sends a wrong
// revealed bit, or proves a false polynomial statement. The verifier must then
// abort. Every run that completes prints "party P: finalize passed".
//
// usage: test_bool_cheat PARTY [MODE ARGS...]; without MODE it runs the honest
// cases: a full AND-gate batch, one more gate than that, a reveal, enough f2k
// multiplications for several chunks of the f2k check, and the four
// polynomial statements.
//   and N CHEAT        N AND gates; the prover lies about gate CHEAT (-1: none)
//   reveal LIE         reveal an AND output; the prover lies about it if LIE=1
//   f2k N CHEAT        N f2k multiplications; the prover lies about CHEAT
//   poly KIND HOLDS    a statement of kind inner / eq / eq2 / multi, true if
//                      HOLDS=1 and false if HOLDS=0
//   rollover EXPECTED  AND gates on both sides of a SilentFerret round, with
//                      the prepay EXPECTED (0: each round prepaid as it
//                      starts; otherwise it must cover several rounds)
//   padding            the prover puts a false bit in the padding of a chunk
//   role               a BoolIO whose sender flag contradicts the party
#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
using namespace emp;
using namespace std;

// A prover that deviates at one chosen point. and_block and f2k_mul repeat
// ZKBoolProver's steps, except that at the chosen gate the prover commits to a
// wrong product and keeps its MAC consistent with it, so that only the batch
// check can catch it.
class CheatProver : public ZKBoolProver {
public:
  int64_t cheat_and = -1;   // the AND gate to lie about
  int64_t cheat_f2k = -1;   // the f2k_mul to lie about
  bool lie_reveal = false;  // lie about the first bit of the next reveal

  CheatProver(BoolIO *io, int64_t expected_cots)
      : ZKBoolProver(io, expected_cots) {}

  block and_block(block l, block r) override {
    const bool lie = (int64_t)gid == cheat_and;
    ++gid;
    if (check_cnt == CHECK_SZ) {
      andgate_correctness_check_manage();
      check_cnt = 0;
    }
    block auth;
    ferret->next_n(&auth, 1);
    andgate_left_buffer[check_cnt] = l;
    andgate_right_buffer[check_cnt] = r;
    const bool s = (getLSB(l) && getLSB(r)) != lie;
    io->send_bit(s ^ getLSB(auth));
    auth = with_lsb(auth, s);
    andgate_out_buffer[check_cnt] = auth;
    check_cnt++;
    return auth;
  }

  void reveal_bits(bool *out, int to_party, const block *in, size_t n) override {
    if (!lie_reveal || to_party == ALICE) {
      ZKBoolProver::reveal_bits(out, to_party, in, n);
      return;
    }
    for (size_t i = 0; i < n; ++i) {
      out[i] = getLSB(in[i]);
      io->send_bit(i == 0 ? !out[i] : out[i]);
    }
    auth_hash.put_block(in, (int64_t)n);   // the true MACs
    lie_reveal = false;
  }

  void f2k_mul(F2kAuthValue &out, const F2kAuthValue &a,
               const F2kAuthValue &b) override {
    const bool lie = (f2k_idx_++ == cheat_f2k);
    f2k_init();
    if (f2k_check_cnt == f2k_buffer_sz) {
      static_cast<ZKBoolBase *>(this)->f2k_check_manage();
      f2k_check_cnt = 0;
    }
    if (f2k_authval_cnt == f2k_buffer_sz) f2k_pre_buffer_refill();
    f2k_left_val[f2k_check_cnt] = a.val;
    f2k_left_mac[f2k_check_cnt] = a.mac;
    f2k_rght_val[f2k_check_cnt] = b.val;
    f2k_rght_mac[f2k_check_cnt] = b.mac;
    block v;
    gfmul(a.val, b.val, &v);
    if (lie) v = v ^ makeBlock(0, 1);
    block d = v ^ f2k_auth_buffer[f2k_authval_cnt].val;
    f2k_auth_buffer[f2k_authval_cnt].val = v;
    io->send_data(&d, sizeof(block));
    out.val = v;
    out.mac = f2k_auth_buffer[f2k_authval_cnt].mac;
    f2k_check_cnt++;
    f2k_authval_cnt++;
  }

private:
  int64_t f2k_idx_ = 0;
};

// The prover is a CheatProver (honest unless told where to lie).
static ZKBoolBase *make_engine(int party, BoolIO *io, CheatProver **cheat,
                               int64_t expected_cots = 0) {
  if (party == ALICE) return *cheat = new CheatProver(io, expected_cots);
  *cheat = nullptr;
  return new ZKBoolVerifier(io, expected_cots);
}

static void passed(int party, const string &what) {
  fprintf(stderr, "party %d: finalize passed (%s)\n", party, what.c_str());
}

static void run_and(BoolIO *io, int party, int64_t n, int64_t cheat) {
  CheatProver *cp;
  ZKBoolBase *e = make_engine(party, io, &cp);
  if (cp) cp->cheat_and = cheat;
  const int m = 64;
  bool bits[m];
  for (int i = 0; i < m; ++i) bits[i] = (i * 2654435761u >> 7) & 1;
  block x[m];
  e->feed_bits(x, ALICE, bits, m);
  block acc = e->public_block(false);
  for (int64_t i = 0; i < n; ++i) {
    block w = e->and_block(x[i % m], x[(i * 7 + 3) % m]);
    acc = e->xor_block(acc, w);
    x[i % m] = e->xor_block(x[i % m], w);
  }
  bool r = false;
  e->reveal_bits(&r, PUBLIC, &acc, 1);
  delete e;   // the closing checks
  passed(party, "and " + to_string(n) + " " + to_string(cheat));
}

static void run_reveal(BoolIO *io, int party, bool lie) {
  CheatProver *cp;
  ZKBoolBase *e = make_engine(party, io, &cp);
  if (cp) cp->lie_reveal = lie;
  bool bits[2] = {true, true};
  block x[2];
  e->feed_bits(x, ALICE, bits, 2);
  block w = e->and_block(x[0], x[1]);
  bool r = false;
  e->reveal_bits(&r, PUBLIC, &w, 1);
  expecting(lie || r, "reveal: 1 AND 1 revealed as 0");
  delete e;
  passed(party, lie ? "reveal 1" : "reveal 0");
}

static void run_f2k(BoolIO *io, int party, int64_t n, int64_t cheat) {
  CheatProver *cp;
  ZKBoolBase *e = make_engine(party, io, &cp);
  if (cp) cp->cheat_f2k = cheat;
  F2kAuthValue a = e->f2k_input(makeBlock(0, 3));
  F2kAuthValue acc = a;
  for (int64_t i = 0; i < n; ++i) e->f2k_mul(acc, acc, a);
  delete e;
  passed(party, "f2k " + to_string(n) + " " + to_string(cheat));
}

// One polynomial statement. Both parties derive the same test values from
// `prg`; only the prover's are used.
static void prove_poly(ZKBoolSession &sess, PRG &prg, const string &kind,
                       bool holds) {
  expecting(kind == "inner" || kind == "eq" || kind == "eq2" || kind == "multi",
            "poly: unknown kind");
  auto in = [&](bool b) { return sess.input<ZKBit>(ALICE, b); };
  const int64_t len = 16, len2 = 8;
  if (kind == "inner") {
    bool v[2 * len];
    prg.random_bool(v, 2 * len);
    bool c = false;
    for (int64_t i = 0; i < len; ++i) c ^= v[i] & v[len + i];
    vector<ZKBit> x;
    for (int64_t i = 0; i < 2 * len; ++i) x.push_back(in(v[i]));
    zkp_inner_prdt(sess, x.data(), x.data() + len, holds ? c : !c, len);
  } else if (kind == "eq" || kind == "eq2") {
    // <x, y> = <r, s> (+ rr·ss for eq2). r[0] = 1, and s[0] is set so that
    // the statement holds or fails as asked.
    bool v[2 * len], w[2 * len2];
    prg.random_bool(v, 2 * len);
    prg.random_bool(w, 2 * len2);
    const bool rr = true, ss = true;
    bool lhs = false, rest = (kind == "eq2") && (rr & ss);
    for (int64_t i = 0; i < len; ++i) lhs ^= v[i] & v[len + i];
    for (int64_t j = 1; j < len2; ++j) rest ^= w[j] & w[len2 + j];
    w[0] = true;
    w[len2] = lhs ^ rest ^ !holds;
    vector<ZKBit> x, y;
    for (int64_t i = 0; i < 2 * len; ++i) x.push_back(in(v[i]));
    for (int64_t j = 0; j < 2 * len2; ++j) y.push_back(in(w[j]));
    if (kind == "eq") {
      zkp_inner_prdt_eq(sess, x.data(), x.data() + len, y.data(),
                        y.data() + len2, len, len2);
    } else {
      ZKBit zr = in(rr), zs = in(ss);
      zkp_inner_prdt_eq(sess, x.data(), x.data() + len, y.data(),
                        y.data() + len2, &zr, &zs, len, len2);
    }
  } else {   // multi
    // For each bit position w: Σ_i X[i][w]·Y[i] = R[w]·S. Values stay below
    // 2^15, so the 16-bit signed inputs hold them as is.
    const int64_t n = 15, width = 16;
    uint64_t xv[n], yv;
    prg.random_data_unaligned(xv, sizeof(xv));
    prg.random_data_unaligned(&yv, sizeof(yv));
    for (int64_t i = 0; i < n; ++i) xv[i] &= 0x7FFF;
    yv &= 0x7FFF;
    bool r[width];
    for (int64_t b = 0; b < width; ++b) {
      r[b] = false;
      for (int64_t i = 0; i < n; ++i) r[b] ^= ((xv[i] >> b) & (yv >> i) & 1) != 0;
    }
    if (!holds) r[3] = !r[3];
    vector<ZKInt> X;
    for (int64_t i = 0; i < n; ++i)
      X.push_back(sess.input<ZKInt>(ALICE, (int64_t)xv[i], width));
    ZKInt Y = sess.input<ZKInt>(ALICE, (int64_t)yv, width);
    vector<ZKBit> R;
    for (int64_t b = 0; b < width; ++b) R.push_back(in(r[b]));
    ZKBit S = in(true);
    zkp_inner_prdt_multi(sess, X.data(), &Y, R.data(), &S, n, width);
  }
}

// The statements of `kinds` in one session, all true or all false.
static void run_poly(BoolIO *io, int party, const vector<string> &kinds,
                     bool holds) {
  ZKBoolSession sess(io, party);
  const block seed = makeBlock(0x5eed, 1);
  PRG prg(&seed);
  string what = "poly";
  for (const string &kind : kinds) {
    prove_poly(sess, prg, kind, holds);
    what += " " + kind;
  }
  sess.finalize();
  passed(party, what + (holds ? " 1" : " 0"));
}

// Authenticated inputs up to just before the end of a SilentFerret round, then
// AND gates that cross it while bits are still pending in BoolIO.
static void run_rollover(BoolIO *io, int party, int64_t expected) {
  CheatProver *cp;
  ZKBoolBase *e = make_engine(party, io, &cp, expected);
  SilentFerret *sf = e->ferret;
  // A prepay of several rounds rolls into the next one without wire traffic;
  // a single round would take the same path as streaming.
  if (expected > 0)
    expecting(sf->prepared_capacity() > sf->cots_per_round(),
              "rollover: the prepay must cover more than one round");
  const int64_t target = sf->cots_per_round() - 300;
  const uint64_t round0 = sf->current_round();
  const int64_t chunk = 1 << 20;
  vector<block> wires(chunk);
  auto bits = make_unique<bool[]>(chunk);
  for (int64_t i = 0; i < chunk; ++i) bits[i] = (i * 31u >> 2) & 1;
  for (int64_t fed = 0; fed < target;) {
    const int64_t k = min<int64_t>(chunk, target - fed);
    e->feed_bits(wires.data(), ALICE, bits.get(), (size_t)k);
    fed += k;
  }
  block acc = e->public_block(false);
  for (int64_t i = 0; i < 1000; ++i)
    acc = e->xor_block(acc, e->and_block(wires[i % 4096],
                                         wires[(i * 13 + 5) % 4096]));
  bool r = false;
  e->reveal_bits(&r, PUBLIC, &acc, 1);
  expecting(sf->current_round() != round0,
            "rollover: the proof did not cross a SilentFerret round");
  delete e;
  passed(party, "rollover " + to_string(expected));
}

// The prover sends one bit more than the verifier reads before a byte message,
// so the chunk that the message closes has a false bit in its padding.
static void run_padding(BoolIO *io, int party) {
  const block msg = makeBlock(1, 2);
  if (party == ALICE) {
    for (int i = 0; i < 10; ++i) io->send_bit(true);
    io->send_bit(false);
    io->send_block(&msg, 1);
  } else {
    for (int i = 0; i < 10; ++i) io->recv_bit();
    block got;
    io->recv_block(&got, 1);
  }
  io->flush();
  passed(party, "padding");
}

// A BoolIO whose sender flag contradicts the party: the engine must refuse it
// before any traffic.
static void run_role(IOChannel *netio, int party) {
  BoolIO wrong(netio, party != ALICE);
  ZKBoolSession sess(&wrong, party);
  passed(party, "role");
}

int main(int argc, char **argv) {
  int party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port())
                                : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);
  auto arg = [&](int i) { return i < argc ? atoll(argv[i]) : -1; };
  const string mode = argc > 2 ? argv[2] : "";
  expecting(mode.empty() || mode == "and" || mode == "reveal" ||
                mode == "f2k" || mode == "poly" || mode == "rollover" ||
                mode == "padding" || mode == "role",
            "test_bool_cheat: unknown mode");

  if (mode.empty()) {
    const int64_t full = ZKBoolBase::CHECK_SZ;
    run_and(&io, party, full, -1);       // one full batch, checked at finalize
    run_and(&io, party, full + 1, -1);   // a full batch, then a batch of one
    run_reveal(&io, party, false);
    run_f2k(&io, party, 3000, -1);       // three chunks of the f2k check
    run_poly(&io, party, {"inner", "eq", "eq2", "multi"}, true);
  } else if (mode == "and") {
    run_and(&io, party, arg(3), arg(4));
  } else if (mode == "reveal") {
    run_reveal(&io, party, arg(3) == 1);
  } else if (mode == "f2k") {
    run_f2k(&io, party, arg(3), arg(4));
  } else if (mode == "poly") {
    run_poly(&io, party, {argc > 3 ? argv[3] : ""}, arg(4) == 1);
  } else if (mode == "rollover") {
    run_rollover(&io, party, arg(3));
  } else if (mode == "padding") {
    run_padding(&io, party);
  } else {   // role
    run_role(netio.get(), party);
  }
  return 0;
}
