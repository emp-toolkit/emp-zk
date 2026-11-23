#ifndef EMP_ZK_BOOL_H__
#define EMP_ZK_BOOL_H__

// emp-zk-bool public surface. A proof is driven through a ZKBoolSession
// (zk_session.h) — the public handle that owns the engine, is the I/O boundary,
// and is passed explicitly to every gadget. There is no global backend and no
// setup_zk_bool/finalize_zk_bool; construct a ZKBoolSession and call
// sess.finalize() to run the closing checks.

#include "emp-zk/emp-zk-bool/zk_session.h"
#include "emp-zk/emp-zk-bool/polynomial.h"

namespace emp {
using namespace std;

// --- polynomial-proof entry points -----------------------------------------
// Statement bits/ints are ZKBit/ZKInt; the proof gadget (PolyProof) runs on raw
// authenticated blocks, so these copy the wires out at the boundary (a ZKBit
// carries a context pointer alongside its wire — not layout-castable to block*).
// Every statement is over GF(2), and its proof settles with the session's
// closing checks.

// Proves Σ_{i<len} coeff[i+1]·x[i]·y[i] = coeff[0]; coeff has len + 1 entries.
inline void zkp_poly_deg2(ZKBoolSession &sess, ZKBit *x, ZKBit *y, bool *coeff,
                          int64_t len) {
  expecting(len >= 0, "zkp_poly_deg2: negative length");
  std::vector<block> xb((size_t)len), yb((size_t)len);
  for (int64_t i = 0; i < len; ++i) { xb[i] = bit_block(x[i]); yb[i] = bit_block(y[i]); }
  sess.engine().polyproof->zkp_poly_deg2(xb.data(), yb.data(), coeff, len);
}

// Proves Σ_{i<len} x[i]·y[i] = constant.
inline void zkp_inner_prdt(ZKBoolSession &sess, ZKBit *x, ZKBit *y, bool constant,
                           int64_t len) {
  expecting(len >= 0, "zkp_inner_prdt: negative length");
  std::vector<block> xb((size_t)len), yb((size_t)len);
  for (int64_t i = 0; i < len; ++i) { xb[i] = bit_block(x[i]); yb[i] = bit_block(y[i]); }
  sess.engine().polyproof->zkp_inner_prdt(xb.data(), yb.data(), constant, len);
}

// Proves Σ_{i<len} x[i]·y[i] = Σ_{j<len2} r[j]·s[j].
inline void zkp_inner_prdt_eq(ZKBoolSession &sess, ZKBit *x, ZKBit *y, ZKBit *r,
                              ZKBit *s, int64_t len, int64_t len2) {
  expecting(len >= 0 && len2 >= 0, "zkp_inner_prdt_eq: negative length");
  std::vector<block> xb((size_t)len), yb((size_t)len), rb((size_t)len2), sb((size_t)len2);
  for (int64_t i = 0; i < len; ++i)  { xb[i] = bit_block(x[i]); yb[i] = bit_block(y[i]); }
  for (int64_t i = 0; i < len2; ++i) { rb[i] = bit_block(r[i]); sb[i] = bit_block(s[i]); }
  sess.engine().polyproof->zkp_inner_prdt_eq(xb.data(), yb.data(), rb.data(),
                                             sb.data(), len, len2);
}

// Proves Σ_{i<len} x[i]·y[i] = Σ_{j<len2} r[j]·s[j] + rr·ss.
inline void zkp_inner_prdt_eq(ZKBoolSession &sess, ZKBit *x, ZKBit *y, ZKBit *r,
                              ZKBit *s, ZKBit *rr, ZKBit *ss, int64_t len,
                              int64_t len2) {
  expecting(len >= 0 && len2 >= 0, "zkp_inner_prdt_eq: negative length");
  std::vector<block> xb((size_t)len), yb((size_t)len), rb((size_t)len2), sb((size_t)len2);
  for (int64_t i = 0; i < len; ++i)  { xb[i] = bit_block(x[i]); yb[i] = bit_block(y[i]); }
  for (int64_t i = 0; i < len2; ++i) { rb[i] = bit_block(r[i]); sb[i] = bit_block(s[i]); }
  block rrb = bit_block(*rr), ssb = bit_block(*ss);
  sess.engine().polyproof->zkp_inner_prdt_eq(xb.data(), yb.data(), rb.data(),
                                             sb.data(), &rrb, &ssb, len, len2);
}

// Proves, for every bit position w < in_width,
//   Σ_{i<len} polyx[i][w]·polyy[0][i] = r[w]·s[0]:
// the bits of the matrix polyx (len integers of at least in_width bits) times
// the vector of bits of polyy[0] (at least len bits) equal r scaled by s[0].
// One claim per bit position. It drives PolyProof's public accumulate_* /
// buffer / num members directly, because PolyProof sits below the typed layer.
inline void zkp_inner_prdt_multi(ZKBoolSession &sess, ZKInt *polyx, ZKInt *polyy,
                                 ZKBit *r, ZKBit *s, int64_t len, int64_t in_width) {
  expecting(len >= 0 && in_width >= 0, "zkp_inner_prdt_multi: negative length");
  PolyProof *pp = sess.engine().polyproof;
  const int party = sess.party();
  for (int64_t width = 0; width < in_width; ++width) {
    if (pp->num >= PolyProof::buffer_sz)
      pp->batch_check();
    if (party == ALICE) {
      block A0 = zero_block, A1 = zero_block;
      for (int64_t i = 0; i < len; ++i)
        pp->accumulate_alice(polyx[i][width].w.label, polyy[0][i].w.label, A0, A1);
      pp->accumulate_alice(r[width].w.label, s->w.label, A0, A1);
      pp->buffer[pp->num] = A0;
      pp->buffer1[pp->num] = A1;
    } else {
      block B = zero_block;
      for (int64_t i = 0; i < len; ++i)
        pp->accumulate_bob(polyx[i][width].w.label, polyy[0][i].w.label, B);
      pp->accumulate_bob(r[width].w.label, s->w.label, B);
      pp->buffer[pp->num] = B;
    }
    pp->num++;
  }
}

} // namespace emp

// Multiset permutation-check gadget (the core of VOLE-ZK RAM/ROM), then the
// RAM/ROM and set data structures. All take a ZKBoolSession& explicitly.
#include "emp-zk/emp-zk-bool/zk_perm_proof.h"
#include "emp-zk/emp-zk-bool/zk_set.h"
#include "emp-zk/emp-zk-bool/zk_ram.h"

#endif
