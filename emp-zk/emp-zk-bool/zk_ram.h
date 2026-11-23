#ifndef EMP_ZK_RAM_H__
#define EMP_ZK_RAM_H__

#include "emp-zk/emp-zk-bool/zk_set.h"
#include "emp-zk/emp-zk-bool/zk_perm_proof.h"
#include <limits>
#include <memory>

namespace emp {
using namespace std;

// Zero-knowledge memory via the "two shuffles" construction (Yang–Heath, "Two
// Shuffles Make a RAM", §4). One class serves both read/write RAM and
// read-only ROM; `read_only` selects between them.
//
// Each access reads the old contents (value, address, time) and writes back
// the new contents: a store writes its value, a load (or any ROM lookup) the
// value it read. The reads and writes go into `perm`, and check() proves
// reads ∼ writes (shuffle 1): every read matches a write, and no write is
// read twice. A read could still match some other write to its address, so
// the time field orders them:
//   RAM: a write is stamped with the public clock, and shuffle 2 (a ZKSet
//        membership proof) shows clock − t ∈ {1, …, T} for the time t that a
//        read claims (§4.3, Fig. 9). The difference is computed modulo
//        2^time_sz ≥ 2T, so t lies in the past; as each write is read once, a
//        read then gets the latest write to its address.
//   ROM: values never change, so a lookup only has to reach the setup value.
//        It writes back the version it read plus one (§4.1, Fig. 7). With at
//        most T lookups and versions modulo 2^time_sz > T, the versions of a
//        cell cannot close a cycle: they form one chain from the cell's setup
//        write, and every lookup returns the setup value.
//
// T is the maximum number of read()/write() accesses (not counting init or
// the teardown in check()), enforced on both parties. Operation type is
// public, so no multiplexer multiplication is needed (Fig. 9's op·(w−old)
// term). Usage: init() once, read()/write() up to T times, then check(), before
// the session's finalize(). A read/write ZKRam needs check() even if it was
// never used, since its ZKSet adds the set's setup records at construction.
class ZKRam {
public:
  int party;
  bool read_only;
  int64_t index_sz, val_sz, T;
  // RAM: timestamps 0 … T, with clock − t computed modulo 2^time_sz ≥ 2T.
  // ROM: versions 0 … T, modulo 2^time_sz > T.
  int64_t time_sz;
  int64_t n = 0;                   // number of cells
  int64_t clock = 1;               // public; setup writes are time 0
  // ALICE's copy of the memory: cell i's val_sz bits start at mem[i·val_sz],
  // and last_t[i] is the time of its last write (RAM) or its version (ROM).
  std::unique_ptr<bool[]> mem;
  vector<uint64_t> last_t;
  ZKBoolSession &zk;               // owns the engine; source of runtime input/reveal
  ZKBoolBase *bb;
  ZKPermProof perm;                // shuffle 1: reads ∼ writes
  std::unique_ptr<ZKSet> diffs;    // shuffle 2 (read/write only): clock−t in {1..T}
  vector<F2kAuthValue> elem_;

  ZKRam(ZKBoolSession &sess, int64_t index_sz, int64_t val_sz, int64_t T,
        bool read_only = false)
      : party(sess.party()), read_only(read_only), index_sz(index_sz),
        val_sz(val_sz), T(T), time_sz(time_bits_(index_sz, val_sz, T, read_only)),
        zk(sess), bb(&sess.engine()),
        perm(sess, index_sz + val_sz + time_sz,
             "ZKRam: the reads do not match the writes"),
        scratch_(std::make_unique<bool[]>((size_t)val_sz)) {
    if (!read_only)
      diffs = std::make_unique<ZKSet>(sess, T, time_sz);
  }

  ~ZKRam() {
    expecting(!perm.pending() && !(diffs && diffs->perm.pending()),
              "ZKRam: destroyed before check()");
  }

  // Setup: commit the initial contents and emit a write (x[i], i, 0) per cell.
  void init(const vector<ZKUInt> &content) {
    expecting(!inited_ && !checked_,
              "ZKRam::init: call it once, before read(), write() and check()");
    inited_ = true;
    n = (int64_t)content.size();
    expecting(index_sz >= 63 || n <= (int64_t(1) << index_sz),
              "ZKRam::init: index_sz bits cannot address every cell");
    for (const ZKUInt &c : content)
      expecting(c.width() == val_sz, "ZKRam::init: each cell must be val_sz bits wide");
    if (party == ALICE) {
      mem = std::make_unique<bool[]>((size_t)(n * val_sz));
      last_t.assign((size_t)n, 0);
    }
    ZKUInt time0 = zk.input<ZKUInt>(PUBLIC, 0, time_sz);
    for (int64_t i = 0; i < n; ++i) {
      zk.reveal_bits(row_(i), ALICE, content[i].data(), (size_t)val_sz);
      emit_(/*toA=*/false, content[i],
            zk.input<ZKUInt>(PUBLIC, (uint64_t)i, index_sz), time0);
    }
  }

  ZKUInt read(const ZKUInt &index) { return access_(index, nullptr); }

  void write(const ZKUInt &index, const ZKUInt &value) {
    expecting(!read_only, "ZKRam: write() on read-only memory");
    access_(index, &value);
  }

  // Teardown: read every cell's final state, then run the shuffle(s).
  void check() {
    expecting(!checked_, "ZKRam: check() already ran");
    checked_ = true;
    for (int64_t i = 0; i < n; ++i) {
      // Commit the value, then the time: the order is part of the protocol,
      // so it must not be left to the order in which arguments are evaluated.
      ZKUInt v = input_row_(row_(i));
      ZKUInt t = zk.input<ZKUInt>(ALICE, party == ALICE ? last_t[i] : 0, time_sz);
      emit_(/*toA=*/true, v, zk.input<ZKUInt>(PUBLIC, (uint64_t)i, index_sz), t);
    }
    perm.check_eq();
    if (diffs)
      diffs->check();
  }

private:
  bool inited_ = false;
  bool checked_ = false;
  // The prover's stand-in for a cell that does not exist, and the verifier's
  // buffer for the bits it passes in place of the prover's.
  std::unique_ptr<bool[]> scratch_;

  // The width of the time field. Checks the parameters first, since the
  // members after it are sized from them.
  static int64_t time_bits_(int64_t index_sz, int64_t val_sz, int64_t T,
                            bool read_only) {
    expecting(index_sz >= 1 && index_sz <= 64, "ZKRam: index_sz must be in [1, 64]");
    expecting(val_sz >= 1 && val_sz <= std::numeric_limits<int>::max(),
              "ZKRam: val_sz must be in [1, INT_MAX]");
    expecting(T >= 1 && T < (int64_t(1) << 61), "ZKRam: T must be in [1, 2^61)");
    return ramzk_bits_for(read_only ? T : 2 * T - 1);
  }

  // ALICE's bits of cell i; the scratch row on the verifier, which keeps no
  // copy of the memory.
  bool *row_(int64_t i) {
    return party == ALICE ? mem.get() + i * val_sz : scratch_.get();
  }

  // Commit val_sz prover bits as a value.
  ZKUInt input_row_(const bool *row) {
    std::vector<ZKWire> w = zk.input_bits(ALICE, row, (size_t)val_sz);
    return ZKUInt::from_wires(zk.ctx(), w.data(), (int)val_sz);
  }

  // One access: a load if `value` is null, else a store of *value. Returns the
  // old value.
  ZKUInt access_(const ZKUInt &index, const ZKUInt *value) {
    expecting(inited_, "ZKRam: init() must come before read() and write()");
    expecting(!checked_, "ZKRam: read() or write() after check()");
    expecting(clock <= T, "ZKRam: more than T accesses");
    expecting(index.width() == index_sz, "ZKRam: the index must be index_sz bits wide");
    expecting(value == nullptr || value->width() == val_sz,
              "ZKRam::write: the value must be val_sz bits wide");
    uint64_t ci = zk.reveal(index, ALICE).value_or(0);
    // An index_sz-bit witness can exceed the cell count. Avoid a local OOB by
    // using the scratch row for an out-of-range index and letting the closing
    // permutation check reject (the bogus read matches no write) — a
    // verifier-observed failure rather than a prover-side abort.
    const bool in_range = (ci < (uint64_t)n);
    bool *row = (party == ALICE && in_range) ? row_((int64_t)ci) : scratch_.get();
    uint64_t t_old = (party == ALICE && in_range) ? last_t[ci] : 0;
    ZKUInt old = input_row_(row);
    ZKUInt time_old = zk.input<ZKUInt>(ALICE, t_old, time_sz);

    // RAM: stamp the write with the clock, and prove the read's time is in the
    // past (shuffle 2). ROM: write back the read version plus one.
    ZKUInt time_new = read_only
        ? time_old + zk.input<ZKUInt>(PUBLIC, 1, time_sz)
        : zk.input<ZKUInt>(PUBLIC, (uint64_t)clock, time_sz);
    if (!read_only)
      diffs->prove_member(time_new - time_old);

    // shuffle 1: read the old (value, addr, time), write back the new state.
    emit_(/*toA=*/true, old, index, time_old);
    emit_(/*toA=*/false, value ? *value : old, index, time_new);

    if (value)
      zk.reveal_bits(row, ALICE, value->data(), (size_t)val_sz);
    if (party == ALICE && in_range)
      last_t[ci] = read_only ? t_old + 1 : (uint64_t)clock;
    ++clock;
    return old;
  }

  void emit_(bool toA, const ZKUInt &value, const ZKUInt &index,
             const ZKUInt &time) {
    elem_.clear();
    ramzk_pack_record(bb, {&value, &index, &time}, elem_);
    if (toA)
      perm.add_A(elem_.data());
    else
      perm.add_B(elem_.data());
  }
};

// Read-only memory: ZKRam in ROM mode, where each lookup writes back the
// version it read plus one. T is the maximum number of read() lookups.
// read() returns the looked-up value; write() is illegal.
class ZKROM : public ZKRam {
public:
  ZKROM(ZKBoolSession &sess, int64_t index_sz, int64_t val_sz, int64_t T)
      : ZKRam(sess, index_sz, val_sz, T, /*read_only=*/true) {}
};

} // namespace emp
#endif // EMP_ZK_RAM_H__
