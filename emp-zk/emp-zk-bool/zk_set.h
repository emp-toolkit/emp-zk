#ifndef EMP_ZK_SET_H__
#define EMP_ZK_SET_H__

#include "emp-zk/emp-zk-bool/zk_perm_proof.h"

namespace emp {
using namespace std;

// Zero-knowledge set with membership queries (Yang–Heath, "Two Shuffles Make
// a RAM", §4.2), the ℓ=0 specialization of the version-chain ROM. The set is
// the public range {1, …, T}; prove_member(v) proves the committed value v is
// one of those elements.
//
// Each record is (element, version) packed into one f2k wire. Setup writes
// (e, 0) for every element; a query of v reads (v, ver) and writes
// (v, ver + 1); check() reads every element's final version and proves
// reads ∼ writes. A value outside the range has no setup write, so its
// queries balance only if their versions close a cycle, which takes 2^ver_sz
// queries of it (Lemma 2). At most max_queries < 2^ver_sz queries are allowed,
// so check() rejects any query of a non-member.
//
// This is the freshness check of the read/write RAM, which proves
// clock − t ∈ {1, …, T} for every access.
//
// The setup writes are added at construction, so check() must run before the
// session's finalize() even if no query was made.
class ZKSet {
public:
  int party;
  int64_t T;             // set is {1, …, T}
  int64_t elem_sz;       // bit width of an element (and of queried values)
  int64_t max_queries;   // the most prove_member() calls allowed
  int64_t ver_sz;        // version field: holds 0 … max_queries
  int64_t queries = 0;   // prove_member() calls so far
  // ALICE: the version of each value in [0, T]. 0 is not an element: its slot
  // only keeps the prover's model in range, and check() rejects its queries.
  vector<uint64_t> ver;
  ZKBoolSession &zk;     // owns the engine; source of runtime input/reveal
  ZKBoolBase *bb;
  ZKPermProof perm;      // A = reads (queries + teardown), B = writes (setup + queries)
  vector<F2kAuthValue> elem_;

  // At most T queries.
  ZKSet(ZKBoolSession &sess, int64_t T, int64_t elem_sz)
      : ZKSet(sess, T, elem_sz, T) {}

  ZKSet(ZKBoolSession &sess, int64_t T, int64_t elem_sz, int64_t max_queries)
      : party(sess.party()), T(T), elem_sz(elem_sz), max_queries(max_queries),
        ver_sz(version_bits_(T, elem_sz, max_queries)), zk(sess),
        bb(&sess.engine()),
        perm(sess, elem_sz + ver_sz, "ZKSet: the queries do not match the set") {
    if (party == ALICE)
      ver.assign((size_t)T + 1, 0);
    // Setup: write (e, version 0) for every element of the public range.
    ZKUInt ver0 = zk.input<ZKUInt>(PUBLIC, 0, ver_sz);
    for (int64_t e = 1; e <= T; ++e)
      emit_(/*toA=*/false, zk.input<ZKUInt>(PUBLIC, (uint64_t)e, elem_sz), ver0);
  }

  ~ZKSet() { expecting(!perm.pending(), "ZKSet: destroyed before check()"); }

  // Prove that the committed value v ∈ {1, …, T}. Appends a (v, ver) read and
  // a (v, ver+1) write, mirroring a ROM lookup of key v.
  void prove_member(const ZKUInt &v) {
    expecting(!checked_, "ZKSet: prove_member() after check()");
    ++queries;
    expecting(queries <= max_queries, "ZKSet: more than max_queries queries");
    expecting(v.width() == elem_sz,
              "ZKSet::prove_member: the value must be elem_sz bits wide");
    uint64_t e = zk.reveal(v, ALICE).value_or(0);
    // ver is indexed by the queried value (size T+1). A value above T reads
    // version 0 (no local OOB), and check() rejects the query, since no setup
    // write backs it.
    const bool in_range = (e <= (uint64_t)T);
    uint64_t vv = (party == ALICE && in_range) ? ver[e] : 0;
    ZKUInt ver_old = zk.input<ZKUInt>(ALICE, vv, ver_sz);
    emit_(/*toA=*/true, v, ver_old);                                    // read
    emit_(/*toA=*/false, v, ver_old + zk.input<ZKUInt>(PUBLIC, 1, ver_sz)); // write
    if (party == ALICE && in_range)
      ver[e] = vv + 1;
  }

  // Teardown: read every element's final version, then prove reads ∼ writes.
  void check() {
    expecting(!checked_, "ZKSet: check() already ran");
    checked_ = true;
    for (int64_t e = 1; e <= T; ++e) {
      uint64_t vv = (party == ALICE) ? ver[e] : 0;
      emit_(/*toA=*/true, zk.input<ZKUInt>(PUBLIC, (uint64_t)e, elem_sz),
            zk.input<ZKUInt>(ALICE, vv, ver_sz));
    }
    perm.check_eq();
  }

private:
  bool checked_ = false;

  // The width of the version field, from max_queries. Checks the parameters
  // first, since the members after it are sized from them.
  static int64_t version_bits_(int64_t T, int64_t elem_sz, int64_t max_queries) {
    expecting(T >= 1 && T < (int64_t(1) << 61), "ZKSet: T must be in [1, 2^61)");
    expecting(elem_sz >= ramzk_bits_for(T),
              "ZKSet: elem_sz bits cannot hold every element of the set");
    expecting(elem_sz <= 64, "ZKSet: elem_sz must be at most 64");
    expecting(max_queries >= 1 && max_queries < (int64_t(1) << 61),
              "ZKSet: max_queries must be in [1, 2^61)");
    return ramzk_bits_for(max_queries);
  }

  void emit_(bool toA, const ZKUInt &element, const ZKUInt &version) {
    elem_.clear();
    ramzk_pack_record(bb, {&element, &version}, elem_);
    if (toA)
      perm.add_A(elem_.data());
    else
      perm.add_B(elem_.data());
  }
};

} // namespace emp
#endif // EMP_ZK_SET_H__
