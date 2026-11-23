// The RAM, ROM, set and permutation proofs against a cheating prover, and
// their honest runs. The cheating prover edits its own copy of the memory
// (ZKRam::mem and last_t, which the next access commits as the cell's old
// value and time), its own copy of a public bound, or commits a wrong value
// through the public API. The verifier must then abort. Where a check is on
// public data, only the verifier's copy can fire: the prover lifts its own
// bound, or only the verifier's parameters break the contract. Every run that
// completes prints "party P: finalize passed".
//
// usage: test_ram-zk_cheat PARTY [MODE ARGS...]; without MODE it runs the
// honest cases and checks what the verifier receives from a permutation
// proof: an open it could compute itself, and freshly masked product
// coefficients.
//   ram_future T K     K loads of a one-cell RAM with bound T, where the first
//                      and the last load each read the other's write
//   ram_over           five loads of a RAM with bound T = 4
//   ram_stale          a load that rereads the setup write after a store
//   ram_value          a RAM whose prover changes a stored value
//   ram_wide           the same with a bit above 128 of a 200-bit value
//   rom_loop           a ROM lookup that claims a forged value at its own
//                      write's time
//   rom_cycle          ROM lookups that pass a forged value between them
//   rom_value          a ROM whose prover changes a cell's value
//   rom_over           T + 1 lookups whose versions close a cycle
//   rom_oob            a lookup at a secret index past the last cell
//   set_over           max_queries + 1 queries of 0, whose versions would
//                      close a cycle
//   set_nonmember Q M  Q queries of 0, which is not in {1, 2, 3}, with
//                      max_queries M
//   perm               the multisets {x, x, y} and {x, y, y}
//   perm_wide          three-wire elements that differ only in their last wire
//   product N          N products of five values, the first committed with a
//                      wrong value (N = 1, or a full batch plus one)
//   unchecked WHERE    a RAM whose check() never runs, which finalize()
//                      (WHERE=finalize) or its destructor (WHERE=scope) must
//                      catch
//   contract WHICH     a parameter that breaks a contract: alias, set_width,
//                      payload or width
#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace emp;
using namespace std;

// A channel that forwards to `inner` and, while recording, keeps each message
// that this party receives.
class RecIO : public IOChannel {
public:
  IOChannel *inner;
  bool on = false;
  vector<vector<uint8_t>> log;
  explicit RecIO(IOChannel *in) : inner(in) {}
  void start() { log.clear(); on = true; }
  void stop() { on = false; }
  void flush() override { inner->flush(); }
  bool has_session_root() const override { return inner->has_session_root(); }
  [[nodiscard]] SessionID next_session_id() override {
    return inner->next_session_id();
  }

protected:
  void send_data_internal(const void *d, int64_t n) override {
    inner->send_data(d, n);
  }
  void recv_data_internal(void *d, int64_t n) override {
    inner->recv_data(d, n);
    if (on) log.emplace_back((const uint8_t *)d, (const uint8_t *)d + n);
  }
};

static void passed(int party, const string &what) {
  fprintf(stderr, "party %d: finalize passed (%s)\n", party, what.c_str());
}

// ALICE only: the next access to `cell` commits (v, t) as its old value and
// time.
static void steer(ZKRam &m, int64_t cell, uint64_t v, uint64_t t) {
  if (m.party != ALICE) return;
  for (int64_t b = 0; b < m.val_sz; ++b)
    m.mem[cell * m.val_sz + b] = b < 64 && ((v >> b) & 1);
  m.last_t[cell] = t;
}

static uint64_t load(ZKBoolSession &sess, ZKRam &m, uint64_t idx,
                     int owner = PUBLIC) {
  ZKUInt got = m.read(sess.input<ZKUInt>(owner, idx, m.index_sz));
  return sess.reveal(got, PUBLIC).value_or(~0ull);
}

static vector<ZKUInt> cells(ZKBoolSession &sess, const vector<uint64_t> &v,
                            int64_t val_sz) {
  vector<ZKUInt> out;
  for (uint64_t x : v) out.push_back(sess.input<ZKUInt>(ALICE, x, val_sz));
  return out;
}

// One authenticated f2k wire holding the 64-bit value v.
static F2kAuthValue wire(ZKBoolSession &sess, uint64_t v) {
  ZKUInt z = sess.input<ZKUInt>(ALICE, v, 64);
  vector<F2kAuthValue> out;
  ramzk_pack_record(&sess.engine(), {&z}, out);
  return out[0];
}

// A value of val_sz bits with bits set throughout, above bit 128 too.
static ZKUInt wide_value(ZKBoolSession &sess, int val_sz, int seed) {
  auto bits = std::make_unique<bool[]>(val_sz);
  for (int b = 0; b < val_sz; ++b) bits[b] = (b * 7 + seed) % 5 == 0;
  std::vector<ZKWire> w = sess.input_bits(ALICE, bits.get(), val_sz);
  return ZKUInt::from_wires(sess.ctx(), w.data(), val_sz);
}

// Five committed values, factor(i) for i = 0..4, and commitments of their
// product, for the product proof.
static uint64_t factor(int i) { return 3 + 2 * (uint64_t)i; }
struct Product {
  block vals[5], macs[5], good, bad;
};
static Product product(ZKBoolSession &sess) {
  ZKBoolBase &bb = sess.engine();
  Product p;
  for (int i = 0; i < 5; ++i) {
    F2kAuthValue w = wire(sess, factor(i));
    p.vals[i] = w.val;
    p.macs[i] = w.mac;
  }
  const block v = bb.f2k_mul_v(5, p.vals);
  p.good = bb.f2k_pack_v(v);
  // A wrong product under a consistent MAC (on the verifier, the same keys
  // as any committed value).
  p.bad = bb.f2k_pack_v(sess.party() == ALICE ? v ^ makeBlock(0, 1) : v);
  return p;
}

// ---- honest runs ----------------------------------------------------------

// T accesses of a RAM: a store every third access, loads otherwise, each load
// checked against a cleartext copy of the memory. `owner` owns the indices.
static void honest_ram(ZKBoolSession &sess, int64_t T, int ncells, int owner) {
  const int64_t index_sz = ramzk_bits_for(ncells - 1), val_sz = 16;
  vector<uint64_t> model(ncells);
  for (int i = 0; i < ncells; ++i) model[i] = 100 + i;
  ZKRam ram(sess, index_sz, val_sz, T);
  ram.init(cells(sess, model, val_sz));
  for (int64_t c = 0; c < T; ++c) {
    const uint64_t i = (uint64_t)(c * 7) % ncells;
    if (c % 3 == 0) {
      model[i] = (uint64_t)(c * 40503) & 0xFFFF;
      ram.write(sess.input<ZKUInt>(owner, i, index_sz),
                sess.input<ZKUInt>(ALICE, model[i], val_sz));
    } else {
      expecting(load(sess, ram, i, owner) == model[i],
                "honest RAM: a load returned a wrong value");
    }
  }
  ram.check();
}

static void honest_rom(ZKBoolSession &sess) {
  const int64_t T = 5;
  ZKROM rom(sess, 2, 8, T);
  rom.init(cells(sess, {0, 2, 4}, 8));
  for (int64_t c = 0; c < T; ++c)
    expecting(load(sess, rom, c % 3) == 2 * (uint64_t)(c % 3),
              "honest ROM: a lookup returned a wrong value");
  rom.check();
}

// A RAM whose values have bits set above bit 64.
static void honest_wide(ZKBoolSession &sess) {
  const int val_sz = 200;
  vector<ZKUInt> data{wide_value(sess, val_sz, 1), wide_value(sess, val_sz, 2)};
  ZKRam ram(sess, 1, val_sz, 3);
  ram.init(data);
  ZKUInt v = wide_value(sess, val_sz, 3);
  ram.write(sess.input<ZKUInt>(PUBLIC, 1, 1), v);
  ZKUInt got1 = ram.read(sess.input<ZKUInt>(PUBLIC, 1, 1));
  ZKUInt got0 = ram.read(sess.input<ZKUInt>(PUBLIC, 0, 1));
  ZKBit ok = (got1 == v) & (got0 == data[0]);
  expecting(sess.reveal(ok, PUBLIC).value_or(false),
            "honest wide RAM: a load returned a wrong value");
  ram.check();
}

static void honest_set(ZKBoolSession &sess) {
  ZKSet s(sess, 3, 2, 7);
  for (uint64_t e : {1, 2, 3, 3, 2, 1, 1})
    s.prove_member(sess.input<ZKUInt>(ALICE, e, 2));
  s.check();
}

// Multiplicities, and a payload of three wires per element with enough
// elements that the products run through the product proof.
static void honest_perm(ZKBoolSession &sess) {
  F2kAuthValue x = wire(sess, 7), x2 = wire(sess, 7), y = wire(sess, 9);
  ZKPermProof p(sess, 64);
  p.add_A(x); p.add_A(x2); p.add_A(y);
  p.add_B(y); p.add_B(x); p.add_B(x2);
  p.check_eq();

  const int n = 9, m = 3;
  vector<F2kAuthValue> e((size_t)n * m);
  for (int i = 0; i < n * m; ++i) e[i] = wire(sess, 1000 + 37 * (uint64_t)i);
  ZKPermProof q(sess, 300);
  for (int i = 0; i < n; ++i) {
    q.add_A(&e[(size_t)i * m]);
    q.add_B(&e[(size_t)(n - 1 - i) * m]);
  }
  q.check_eq();
}

// One more product than a batch holds, so the product proof checks a full
// batch before the last one.
static void honest_product_batches(ZKBoolSession &sess) {
  Product p = product(sess);
  RamPolyPrdt pp(&sess.engine());
  for (int64_t i = 0; i <= RamPolyPrdt::buffer_sz; ++i)
    pp.polyPrdt5(p.vals, p.macs, p.good);
  pp.batch_check();
}

// What the verifier receives from a permutation proof. The test gives the
// verifier the witness. The open must be the one block K_A + K_B, which it
// holds already. In each product check, U₁..U₄ minus the unmasked
// coefficients, which it computes from the witness, must be nonzero and must
// differ from one check to the next.
static void check_transcript(ZKBoolSession &sess, RecIO &rec) {
  const int party = sess.party();
  ZKBoolBase &bb = sess.engine();

  // A = {42}, B = {42}, committed separately: one element per side, so the
  // open is the only message of check_eq.
  F2kAuthValue a = wire(sess, 42), b = wire(sess, 42);
  ZKPermProof p(sess, 64);
  p.add_A(a);
  p.add_B(b);
  sess.flush();
  rec.start();
  p.check_eq();
  rec.stop();
  if (party == BOB) {
    const block expect = a.mac ^ b.mac;   // K_A + K_B: r cancels
    expecting(rec.log.size() == 1 && rec.log[0].size() == sizeof(block) &&
                  memcmp(rec.log[0].data(), &expect, sizeof(block)) == 0,
              "transcript: the open is not the verifier's own K_A + K_B");
  }

  // One product of five committed values, checked twice.
  Product prod = product(sess);
  block mask[2][5];
  for (int round = 0; round < 2; ++round) {
    RamPolyPrdt pp(&bb);
    pp.polyPrdt5(prod.vals, prod.macs, prod.good);
    sess.flush();
    block S[5];
    if (party == BOB) {
      // χ₀, as batch_check draws it, and the prover's coefficients c_j of
      // ∏ᵢ (Mᵢ + xᵢ·Y), with Mᵢ = Kᵢ + xᵢ·Δ.
      block seed = bb.check_seed(RamPolyPrdt::kCheckDomain), chi, t;
      PRG(&seed).random_block(&chi, 1);
      block X[5], M[5];
      for (int i = 0; i < 5; ++i) {
        X[i] = makeBlock(0, factor(i));
        gfmul(X[i], bb.delta, &t);
        M[i] = prod.macs[i] ^ t;
      }
      block c[6] = {M[0], X[0]};
      for (int i = 1; i < 5; ++i) {
        block nc[6] = {};
        for (int j = 0; j <= i; ++j) {
          gfmul(c[j], M[i], &t);
          nc[j] = nc[j] ^ t;
          gfmul(c[j], X[i], &t);
          nc[j + 1] = nc[j + 1] ^ t;
        }
        for (int j = 0; j < 6; ++j) c[j] = nc[j];
      }
      // c[5] is the product v; the prover folds M_last = K_last + v·Δ into
      // the coefficient of Y⁴.
      gfmul(c[5], bb.delta, &t);
      c[4] = c[4] ^ prod.good ^ t;
      for (int j = 0; j < 5; ++j) gfmul(chi, c[j], &S[j]);
    }
    rec.start();
    pp.batch_check();
    rec.stop();
    if (party == BOB) {
      const vector<uint8_t> *msg = nullptr;
      int found = 0;
      for (const auto &l : rec.log)
        if (l.size() == 5 * sizeof(block)) { msg = &l; ++found; }
      expecting(found == 1, "transcript: no single five-block product message");
      for (int j = 1; j < 5; ++j) {
        block u;
        memcpy(&u, msg->data() + sizeof(block) * j, sizeof(block));
        mask[round][j] = u ^ S[j];
        expecting(!cmpBlock(&mask[round][j], &zero_block, 1),
                  "transcript: the product check sent an unmasked coefficient");
      }
    }
  }
  if (party == BOB)
    for (int j = 1; j < 5; ++j)
      expecting(!cmpBlock(&mask[0][j], &mask[1][j], 1),
                "transcript: two product checks used the same mask");
}

static void run_honest(BoolIO *io, RecIO &rec, int party) {
  ZKBoolSession sess(io, party);
  honest_ram(sess, 3, 2, PUBLIC);      // 2^time_sz = 8 ≥ 2T, not a power of two
  honest_ram(sess, 1000, 8, PUBLIC);
  honest_ram(sess, 60, 5, ALICE);      // secret indices
  honest_rom(sess);
  honest_wide(sess);
  honest_set(sess);
  honest_perm(sess);
  honest_product_batches(sess);
  check_transcript(sess, rec);
  expecting(!sess.engine().f2k_ready,
            "honest: a permutation proof started the f2k VOLE");
  sess.finalize();
  passed(party, "honest");
}

// ---- deviations -------------------------------------------------------------

static void run_ram_future(BoolIO *io, int party, int64_t T, int64_t K) {
  ZKBoolSession sess(io, party);
  const uint64_t x = 5, y = 0xAB;
  ZKRam ram(sess, 1, 8, T);
  ram.init(cells(sess, {x}, 8));
  for (int64_t c = 1; c <= K; ++c) {
    if (c == 1) steer(ram, 0, y, (uint64_t)K);   // the write of load K
    if (c == 2) steer(ram, 0, x, 0);             // the setup write
    if (c == K) steer(ram, 0, y, 1);             // the write of load 1
    load(sess, ram, 0);
  }
  steer(ram, 0, x, (uint64_t)(K - 1));
  ram.check();
  sess.finalize();
  passed(party, "ram_future");
}

// Five loads at T = 4: loads 1 and 5 each read the other's write, with
// differences (1 − 5) mod 8 = 4 and 5 − 1 = 4 in {1, …, 4}. The prover lifts
// its own bounds, so only the verifier's access bound can stop it.
static void run_ram_over(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  const uint64_t x = 5, y = 0xAB;
  ZKRam ram(sess, 1, 8, 4);
  if (party == ALICE) {
    ram.T = 5;
    ram.diffs->max_queries = 5;
  }
  ram.init(cells(sess, {x}, 8));
  for (int64_t c = 1; c <= 5; ++c) {
    if (c == 1) steer(ram, 0, y, 5);
    if (c == 2) steer(ram, 0, x, 0);
    if (c == 5) steer(ram, 0, y, 1);
    load(sess, ram, 0);
  }
  steer(ram, 0, x, 4);
  ram.check();
  sess.finalize();
  passed(party, "ram_over");
}

// A store of 9, then a load that reads the setup write (5, time 0) again. Its
// difference 2 − 0 is in {1, 2}, so only shuffle 1, where each write is read
// once, rejects it.
static void run_ram_stale(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKRam ram(sess, 1, 8, 2);
  ram.init(cells(sess, {5}, 8));
  ram.write(sess.input<ZKUInt>(PUBLIC, 0, 1), sess.input<ZKUInt>(ALICE, 9, 8));
  steer(ram, 0, 5, 0);
  load(sess, ram, 0);
  steer(ram, 0, 9, 1);
  ram.check();
  sess.finalize();
  passed(party, "ram_stale");
}

static void run_ram_value(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKRam ram(sess, 1, 8, 2);
  ram.init(cells(sess, {5, 6}, 8));
  steer(ram, 0, 7, 0);   // cell 0 holds 5
  load(sess, ram, 0);
  load(sess, ram, 1);
  ram.check();
  sess.finalize();
  passed(party, "ram_value");
}

// The prover flips bit 150 of a 200-bit cell, which lands in the second wire
// of the record.
static void run_ram_wide(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  const int val_sz = 200;
  ZKRam ram(sess, 1, val_sz, 1);
  ram.init({wide_value(sess, val_sz, 1)});
  if (party == ALICE) ram.mem[150] = !ram.mem[150];
  ram.read(sess.input<ZKUInt>(PUBLIC, 0, 1));
  ram.check();
  sess.finalize();
  passed(party, "ram_wide");
}

static void run_rom_loop(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKROM rom(sess, 2, 8, 4);
  rom.init(cells(sess, {10, 11, 12, 13}, 8));
  load(sess, rom, 1);
  steer(rom, 2, 0xAB, (uint64_t)rom.clock);
  load(sess, rom, 2);
  steer(rom, 2, 12, 0);
  load(sess, rom, 3);
  rom.check();
  sess.finalize();
  passed(party, "rom_loop");
}

static void run_rom_cycle(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKROM rom(sess, 1, 8, 3);
  rom.init(cells(sess, {5}, 8));
  steer(rom, 0, 9, 3); load(sess, rom, 0);
  steer(rom, 0, 5, 0); load(sess, rom, 0);
  steer(rom, 0, 9, 1); load(sess, rom, 0);
  steer(rom, 0, 5, 2);
  rom.check();
  sess.finalize();
  passed(party, "rom_cycle");
}

static void run_rom_value(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKROM rom(sess, 1, 8, 2);
  rom.init(cells(sess, {5, 6}, 8));
  steer(rom, 0, 7, 0);   // cell 0 holds 5
  load(sess, rom, 0);
  load(sess, rom, 1);
  rom.check();
  sess.finalize();
  passed(party, "rom_value");
}

// With T = 3 the versions count modulo 4, so four lookups claiming a forged
// value at versions 0, 1, 2, 3 write versions 1, 2, 3, 0: a closed cycle. The
// prover lifts its own bound, so only the verifier's can stop it.
static void run_rom_over(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKROM rom(sess, 1, 8, 3);
  if (party == ALICE) rom.T = 4;
  rom.init(cells(sess, {5}, 8));
  for (uint64_t t = 0; t < 4; ++t) {
    steer(rom, 0, 9, t);
    load(sess, rom, 0);
  }
  steer(rom, 0, 5, 0);
  rom.check();
  sess.finalize();
  passed(party, "rom_over");
}

// A secret index past the last cell: the prover reads its scratch row, and
// no write backs the read.
static void run_rom_oob(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKROM rom(sess, 2, 8, 2);
  rom.init(cells(sess, {5, 6, 7}, 8));
  load(sess, rom, 3, ALICE);
  rom.check();
  sess.finalize();
  passed(party, "rom_oob");
}

// T = 3 and the default max_queries = 3 give 2-bit versions, so four queries
// of 0, with versions 0, 1, 2, 3, would close a cycle. The prover lifts its
// own bound, so only the verifier's can stop it.
static void run_set_over(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  ZKSet s(sess, 3, 2);
  if (party == ALICE) s.max_queries = 4;
  for (int i = 0; i < 4; ++i)
    s.prove_member(sess.input<ZKUInt>(ALICE, 0, 2));
  s.check();
  sess.finalize();
  passed(party, "set_over");
}

static void run_set_nonmember(BoolIO *io, int party, int64_t q, int64_t max_q) {
  ZKBoolSession sess(io, party);
  ZKSet s(sess, 3, 2, max_q);
  for (int64_t i = 0; i < q; ++i)
    s.prove_member(sess.input<ZKUInt>(ALICE, 0, 2));
  s.check();
  sess.finalize();
  passed(party, "set_nonmember");
}

static void run_perm(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  F2kAuthValue x = wire(sess, 7), x2 = wire(sess, 7), y = wire(sess, 9),
               y2 = wire(sess, 9);
  ZKPermProof p(sess, 64);
  p.add_A(x); p.add_A(x2); p.add_A(y);
  p.add_B(x); p.add_B(y); p.add_B(y2);
  p.check_eq();
  sess.finalize();
  passed(party, "perm");
}

// B is A reversed, except that one element's last wire differs.
static void run_perm_wide(BoolIO *io, int party) {
  ZKBoolSession sess(io, party);
  const int n = 5, m = 3;
  vector<F2kAuthValue> e((size_t)n * m);
  for (int i = 0; i < n * m; ++i) e[i] = wire(sess, 1000 + 37 * (uint64_t)i);
  F2kAuthValue changed[3] = {e[(size_t)(n - 1) * m], e[(size_t)(n - 1) * m + 1],
                             wire(sess, 4242)};
  ZKPermProof p(sess, 300);
  for (int i = 0; i < n; ++i) {
    p.add_A(&e[(size_t)i * m]);
    p.add_B(i == 0 ? changed : &e[(size_t)(n - 1 - i) * m]);
  }
  p.check_eq();
  sess.finalize();
  passed(party, "perm_wide");
}

// The wrong product comes first, so with a full batch plus one it is caught
// by the check of the first batch.
static void run_product(BoolIO *io, int party, int64_t n) {
  ZKBoolSession sess(io, party);
  Product p = product(sess);
  RamPolyPrdt pp(&sess.engine());
  pp.polyPrdt5(p.vals, p.macs, p.bad);
  for (int64_t i = 1; i < n; ++i) pp.polyPrdt5(p.vals, p.macs, p.good);
  pp.batch_check();
  sess.finalize();
  passed(party, "product");
}

static void run_unchecked(BoolIO *io, int party, const string &where) {
  ZKBoolSession sess(io, party);
  // The prover keeps its RAM out of its own checks: it leaks its copy and
  // uncounts the RAM's two permutation proofs, so only the verifier's checks
  // can fire.
  ZKRam *ram = new ZKRam(sess, 1, 8, 2);
  ram->init(cells(sess, {5}, 8));
  load(sess, *ram, 0);
  if (party == ALICE) sess.engine().unchecked_perm_proofs -= 2;
  else if (where == "scope") delete ram;
  sess.finalize();
  passed(party, "unchecked " + where);
}

// Only the verifier's parameters break the contract, so only its check can
// fire.
static void run_contract(BoolIO *io, int party, const string &which) {
  ZKBoolSession sess(io, party);
  const bool bob = party == BOB;
  if (which == "alias") {
    vector<ZKUInt> data = cells(sess, {5, 6, 7}, 8);
    if (!bob) data.pop_back();
    ZKRam ram(sess, 1, 8, 2);   // a 1-bit index names two cells, not three
    ram.init(data);
    load(sess, ram, 0);
    ram.check();
  } else if (which == "set_width") {
    ZKSet s(sess, 4, bob ? 2 : 3);   // 4 needs three bits
    s.check();
  } else if (which == "payload") {
    ZKPermProof p(sess, bob ? 0 : 64);
    p.check_eq();
  } else {                           // width
    ZKRam ram(sess, 1, 8, 2);
    ram.init(cells(sess, {5}, 8));
    ram.read(sess.input<ZKUInt>(PUBLIC, 0, bob ? 2 : 1));
    ram.check();
  }
  sess.finalize();
  passed(party, "contract " + which);
}

int main(int argc, char **argv) {
  int party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port())
                                : NetIO::connect(peer_ip(), peer_port());
  RecIO rec(netio.get());
  BoolIO io(&rec, party == ALICE);
  auto arg = [&](int i) { return i < argc ? atoll(argv[i]) : -1; };
  const string mode = argc > 2 ? argv[2] : "";
  const string word = argc > 3 ? argv[3] : "";
  expecting(mode.empty() || mode == "ram_future" || mode == "ram_over" ||
                mode == "ram_stale" || mode == "ram_value" ||
                mode == "ram_wide" || mode == "rom_loop" ||
                mode == "rom_cycle" || mode == "rom_value" ||
                mode == "rom_over" || mode == "rom_oob" ||
                mode == "set_over" || mode == "set_nonmember" ||
                mode == "perm" || mode == "perm_wide" || mode == "product" ||
                mode == "unchecked" || mode == "contract",
            "test_ram-zk_cheat: unknown mode");

  if (mode.empty()) run_honest(&io, rec, party);
  else if (mode == "ram_future") run_ram_future(&io, party, arg(3), arg(4));
  else if (mode == "ram_over") run_ram_over(&io, party);
  else if (mode == "ram_stale") run_ram_stale(&io, party);
  else if (mode == "ram_value") run_ram_value(&io, party);
  else if (mode == "ram_wide") run_ram_wide(&io, party);
  else if (mode == "rom_loop") run_rom_loop(&io, party);
  else if (mode == "rom_cycle") run_rom_cycle(&io, party);
  else if (mode == "rom_value") run_rom_value(&io, party);
  else if (mode == "rom_over") run_rom_over(&io, party);
  else if (mode == "rom_oob") run_rom_oob(&io, party);
  else if (mode == "set_over") run_set_over(&io, party);
  else if (mode == "set_nonmember") run_set_nonmember(&io, party, arg(3), arg(4));
  else if (mode == "perm") run_perm(&io, party);
  else if (mode == "perm_wide") run_perm_wide(&io, party);
  else if (mode == "product") run_product(&io, party, arg(3));
  else if (mode == "unchecked") run_unchecked(&io, party, word);
  else run_contract(&io, party, word);
  return 0;
}
