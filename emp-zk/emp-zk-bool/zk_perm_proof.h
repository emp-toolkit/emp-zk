#ifndef EMP_ZK_PERM_PROOF_H__
#define EMP_ZK_PERM_PROOF_H__

#include "emp-zk/emp-zk-bool/zk_session.h"
#include <array>
#include <limits>
#include <memory>
#include <string>
#include <string_view>

namespace emp {
using namespace std;

// Product proof for five authenticated F(2^128) values, the high-fan-in
// multiply behind the permutation product below (the QuickSilver
// polynomial-proof trick). polyPrdt5 buffers the coefficients of
// P(Δ) = ∏ᵢ(mᵢ + xᵢΔ) for one product, so five factors cost one committed
// value; batch_check folds every buffered product with independent
// coefficients and checks the folded polynomial under a random mask. Owned by
// a ZKPermProof, which allocates it on its first product.
class RamPolyPrdt {
public:
  // The Fiat–Shamir domain of batch_check's coefficients.
  static constexpr std::string_view kCheckDomain = "emp-zk:perm:product-check";
  static constexpr int64_t buffer_sz = 1 << 16;

  ZKBoolBase *bb;
  int party;
  BoolIO *io;
  block delta[4];   // BOB: Δ, Δ², Δ³, Δ⁴
  // ALICE: per-product coefficients of Δ⁰..Δ⁴; BOB: only [0] = poly(Δ).
  std::array<std::vector<block>, 5> buffer;
  int64_t num = 0;

  explicit RamPolyPrdt(ZKBoolBase *bb) : bb(bb), party(bb->party), io(bb->io) {
    if (party == ALICE) {
      for (auto &b : buffer)
        b.resize(buffer_sz);
    } else {
      buffer[0].resize(buffer_sz);
      delta[0] = bb->delta;
      for (int i = 1; i < 4; ++i)
        gfmul(delta[i - 1], delta[0], delta + i);
    }
  }

  ~RamPolyPrdt() {
    expecting(num == 0,
              "RamPolyPrdt: destroyed with products that batch_check() never checked");
  }

  // Check every buffered product. The coefficients χᵢ are a PRG expansion of
  // the check seed, so a batch with a wrong product folds to a nonzero error
  // except with probability 2⁻¹²⁸. That error sits on Δ⁵, which none of the
  // prover's five coefficients reaches, so it then passes only if Δ is a root
  // of a nonzero polynomial of degree 5: about 2⁻¹²⁴ in all.
  //
  // The prover's coefficients U_j are masked by A*(Y) = Σₕ (m_h + u_h·Y)·Y^(h−1),
  // h = 1..4, built from four random authenticated values (m_h = k_h + u_h·Δ).
  // The map (u₁..u₄) → (U₁..U₄) is triangular with unit diagonal, so U₁..U₄
  // are uniform, and U₀ is fixed by the check. The verifier adds
  // B* = A*(Δ) = Σₕ k_h·Δ^(h−1), which it computes from its keys.
  void batch_check() {
    if (num == 0)
      return;
    io->flush();
    block seed = bb->check_seed(kCheckDomain);
    std::vector<block> chi(num);
    PRG(&seed).random_block(chi.data(), num);
    block u[4], mk[4];
    draw_mask_(u, mk);
    if (party == ALICE) {
      block U[5];
      for (int j = 0; j < 5; ++j)
        vector_inn_prdt_sum_red(&U[j], chi.data(), buffer[j].data(), num);
      U[0] = U[0] ^ mk[0];
      for (int h = 1; h < 4; ++h)
        U[h] = U[h] ^ u[h - 1] ^ mk[h];
      U[4] = U[4] ^ u[3];
      io->send_data(U, sizeof(U));
      io->flush();
    } else {
      block B, t;
      vector_inn_prdt_sum_red(&B, chi.data(), buffer[0].data(), num);
      B = B ^ mk[0];
      for (int h = 1; h < 4; ++h) {
        gfmul(mk[h], delta[h - 1], &t);
        B = B ^ t;
      }
      block U[5];
      io->recv_data(U, sizeof(U));
      block W = U[0];
      for (int j = 1; j < 5; ++j) {
        gfmul(U[j], delta[j - 1], &t);
        W = W ^ t;
      }
      expecting(cmpBlock(&W, &B, 1) == 1, "ZKPermProof: product check fails");
    }
    num = 0;
  }

  // Buffer the proof that m_last commits the product x[0]·…·x[4] of five
  // committed values (the prover's cleartext x and MACs m; the verifier's keys
  // m, its x unused). Both sides expand
  //
  //     P(Δ) = (m_0 + x_0 Δ)(m_1 + x_1 Δ) … (m_4 + x_4 Δ),
  //
  // whose Δ⁵ coefficient is the cleartext product v. The prover buffers the
  // coefficients of Δ⁰..Δ⁴, with m_last folded into the one of Δ⁴; the
  // verifier buffers P(Δ) + k_last·Δ⁴. If m_last commits some v' ≠ v, the two
  // differ by (v + v')·Δ⁵. Products of fewer factors are not offered: their
  // error would sit on a power of Δ that the prover's coefficients cancel.
  //
  // ALICE expands the polynomial iteratively,
  //   poly_{i+1}[j] = poly_i[j]·m_{i+1} + poly_i[j-1]·x_{i+1};
  // BOB stores only ∏ᵢ mᵢ + m_last·Δ⁴.
  void polyPrdt5(const block *x, const block *m, const block &m_last) {
    constexpr int N = 5;
    if (num >= buffer_sz) batch_check();

    if (party == ALICE) {
      block poly[N + 1] = {};
      block tmp;

      // Hand-expand the 2-term init: poly = (m_0 + x_0 Δ)(m_1 + x_1 Δ).
      gfmul(m[0], m[1], &poly[0]);
      gfmul(m[0], x[1], &poly[1]);
      gfmul(x[0], m[1], &tmp);
      poly[1] ^= tmp;
      gfmul(x[0], x[1], &poly[2]);

      // Multiply in (m_i + x_i Δ) for i = 2 .. N-1.
      for (int i = 2; i < N; ++i) {
        block new_poly[N + 1] = {};
        // At the last step the new top coefficient poly[N-1]·x_i would be the
        // all-x term x_0·…·x_4 = v, which the caller already has; skip it.
        const int top = (i == N - 1) ? (N - 1) : (i + 1);
        for (int j = 0; j <= top; ++j) {
          if (j <= i)
            gfmul(poly[j], m[i], &new_poly[j]);
          if (j >= 1) {
            gfmul(poly[j - 1], x[i], &tmp);
            new_poly[j] ^= tmp;
          }
        }
        for (int j = 0; j <= top; ++j) poly[j] = new_poly[j];
      }

      for (int j = 0; j < N - 1; ++j) buffer[j][num] = poly[j];
      buffer[N - 1][num] = poly[N - 1] ^ m_last;
    } else {
      block prod = m[0];
      for (int i = 1; i < N; ++i) gfmul(prod, m[i], &prod);
      block adj;
      gfmul(m_last, delta[N - 2], &adj);   // delta[3] = Δ⁴
      buffer[0][num] = prod ^ adj;
    }
    num++;
  }

private:
  // Four random authenticated F(2^128) values, each packed from 128 COTs as
  // Σ·Xⁱ, the way the AND-gate check builds its mask: the prover gets (u_h,
  // m_h) and the verifier k_h, with m_h = k_h + u_h·Δ.
  void draw_mask_(block u[4], block mk[4]) {
    block cot[128];
    for (int h = 0; h < 4; ++h) {
      bb->ferret->next_n(cot, 128);
      bb->pack.packing(&mk[h], cot);
      if (party == ALICE) {
        bool bits[128];
        for (int i = 0; i < 128; ++i) bits[i] = getLSB(cot[i]);
        bb->pack.packing(&u[h], bits);
      }
    }
  }
};

// Zero-knowledge multiset permutation check over authenticated elements, the
// core gadget for VOLE-ZK RAM/ROM (Yang–Heath, "Two Shuffles Make a RAM",
// §2.2). The prover commits two multisets A and B; the proof passes iff A is a
// permutation of B, and the verifier learns nothing else.
//
// Method: encode each multiset as a polynomial with its elements as roots,
//   p_A(X) = ∏_i (X − a_i),   p_B(X) = ∏_i (X − b_i),
// and test p_A = p_B at a challenge r by Schwartz–Zippel. Both products are
// evaluated in zero knowledge four factors at a time through the product
// proof (RamPolyPrdt), and the difference of the two product wires is opened
// as zero.
//
// Payload size (bits) is fixed at construction. ≤128 bits → each element is
// one F(2^128) wire and the product runs directly. Larger → an element spans
// m = ⌈bits/128⌉ wires that are *compressed* to one wire by the random linear
// combination Σ_j c_j·B_j with a coefficient vector c (a local op —
// f2k_mul_const / f2k_add — no VOLE, no comms). As a polynomial in (c, X), the
// difference of the two products has degree |A|, so a false claim passes with
// probability about |A|/2^128, plus the product check's error.
//
// Compression is decoupled from the final check: add_* buffers raw (multi-
// wire) elements; compress() folds the current buffer into the compressed
// lists; the equality test (check_eq) runs once at the end. Each compress()
// draws a FRESH coefficient by Fiat–Shamir *at compress time* — bound to the
// transcript, so the just-committed elements being folded are bound (this is
// what makes it sound). Consequently elements folded in different compress()
// calls collapse under different coefficients and are NOT comparable across
// calls: every element that must match another in this permutation has to be
// folded in the same compress() call. The default flow (let check_eq() fold
// everything once, at the end) satisfies this for a whole-stream check.
// Several compress() calls stay sound, since each fold's coefficients are
// drawn after its elements are committed; the |A|/2^128 bound above is
// derived for a single fold.
//
// Records stay unchecked until check_eq() runs. The session's finalize()
// aborts while any permutation proof holds unchecked records, and so does the
// destructor of one that does.
class ZKPermProof {
public:
  // Fiat–Shamir domains of the fold coefficients and of the challenge r.
  static constexpr std::string_view kFoldDomain = "emp-zk:perm:fold";
  static constexpr std::string_view kChallengeDomain = "emp-zk:perm:challenge";

  ZKBoolBase *bb;
  int64_t n_blocks;   // wires per element = ⌈bits/128⌉

  // Pending raw elements (n_blocks wires each), awaiting compression. For
  // n_blocks == 1 add_* writes straight to the compressed lists and these
  // stay empty.
  std::vector<F2kAuthValue> a_pending, b_pending;
  // Collapsed single-wire elements; the permutation product runs over these.
  std::vector<F2kAuthValue> a_elems_compressed, b_elems_compressed;

  // Product proof for eval_'s batched factors. Created on the first product,
  // batch-checked in check_eq.
  std::unique_ptr<RamPolyPrdt> polyprdt;

  // `mismatch` is the verifier's abort message when A is not a permutation of
  // B; the RAM, ROM and set name themselves in theirs.
  explicit ZKPermProof(ZKBoolSession &sess, int64_t payload_bits = 128,
                       std::string_view mismatch =
                           "ZKPermProof: A is not a permutation of B")
      : bb(&sess.engine()), n_blocks(blocks_for_(payload_bits)),
        mismatch_(mismatch) {}

  ~ZKPermProof() {
    expecting(!pending_,
              "ZKPermProof: destroyed with records that check_eq() never checked");
  }

  // Append one element. The pointer form reads n_blocks consecutive wires;
  // the reference form is for single-wire payloads (≤128 bits).
  void add_A(const F2kAuthValue *e) {
    mark_pending_();
    if (n_blocks == 1) a_elems_compressed.push_back(*e);
    else a_pending.insert(a_pending.end(), e, e + n_blocks);
  }
  void add_B(const F2kAuthValue *e) {
    mark_pending_();
    if (n_blocks == 1) b_elems_compressed.push_back(*e);
    else b_pending.insert(b_pending.end(), e, e + n_blocks);
  }
  void add_A(const F2kAuthValue &e) {
    expecting(n_blocks == 1, "ZKPermProof::add_A: an element spans more than one wire");
    add_A(&e);
  }
  void add_B(const F2kAuthValue &e) {
    expecting(n_blocks == 1, "ZKPermProof::add_B: an element spans more than one wire");
    add_B(&e);
  }

  // Whether records added since the last check_eq() are still unchecked.
  bool pending() const { return pending_; }

  // Fold the pending multi-wire elements into the compressed lists, drawing a
  // fresh coefficient vector by Fiat–Shamir now (so it binds the committed
  // elements being folded). No-op for single-wire payloads or an empty
  // buffer. Matched elements must be folded in the same call (see class doc).
  void compress() {
    if (n_blocks == 1 || (a_pending.empty() && b_pending.empty())) return;
    bb->io->flush();
    block seed = bb->check_seed(kFoldDomain);
    std::vector<block> coeff(n_blocks);
    PRG(&seed).random_block(coeff.data(), n_blocks);
    fold_(a_pending, coeff.data(), a_elems_compressed);
    fold_(b_pending, coeff.data(), b_elems_compressed);
    a_pending.clear();
    b_pending.clear();
  }

  // Prove A ∼ B. Folds any remaining elements, then runs the product test at
  // a fresh challenge r. On the verifier this aborts on failure.
  // Resets state so the object can be reused.
  void check_eq() {
    compress();
    expecting(a_elems_compressed.size() == b_elems_compressed.size(),
              "ZKPermProof: |A| != |B|");
    if (!a_elems_compressed.empty()) {
      bb->io->flush();
      const block r = bb->check_seed(kChallengeDomain);
      F2kAuthValue pa = eval_(a_elems_compressed, r);
      F2kAuthValue pb = eval_(b_elems_compressed, r);
      open_eq_(pa, pb);
      if (polyprdt) polyprdt->batch_check();   // verify the batched products
    }
    a_pending.clear();
    b_pending.clear();
    a_elems_compressed.clear();
    b_elems_compressed.clear();
    if (pending_) {
      pending_ = false;
      --bb->unchecked_perm_proofs;
    }
  }

private:
  std::string mismatch_;
  bool pending_ = false;

  // ⌈payload_bits / 128⌉, after checking payload_bits.
  static int64_t blocks_for_(int64_t payload_bits) {
    expecting(payload_bits >= 1 && payload_bits <= std::numeric_limits<int>::max(),
              "ZKPermProof: payload_bits must be in [1, INT_MAX]");
    return (payload_bits + 127) / 128;
  }

  void mark_pending_() {
    if (pending_) return;
    pending_ = true;
    ++bb->unchecked_perm_proofs;
  }

  // out = f[0]·f[1]·f[2]·f[3]·f[4]. ALICE's f2k_mul_v gives the cleartext
  // product, BOB's gives zero; f2k_pack_v commits it in MAC layout, and the
  // owned product proof checks the commitment.
  void mul5_(F2kAuthValue &out, const F2kAuthValue (&f)[5]) {
    if (!polyprdt)
      polyprdt = std::make_unique<RamPolyPrdt>(bb);
    block vals[5], macs[5];
    for (int i = 0; i < 5; ++i) {
      vals[i] = f[i].val;
      macs[i] = f[i].mac;
    }
    const block v = bb->f2k_mul_v(5, vals);
    const block m = bb->f2k_pack_v(v);
    polyprdt->polyPrdt5(vals, macs, m);
    out.val = v;
    out.mac = m;
  }

  // Collapse each m-wire element to one: out_i = Σ_j coeff_j · elem_i[j].
  // Public coeffs ⇒ all local (f2k_mul_const + f2k_add), no VOLE/comms.
  void fold_(const std::vector<F2kAuthValue> &flat, const block *coeff,
             std::vector<F2kAuthValue> &out) {
    const int64_t cnt = (int64_t)flat.size() / n_blocks;
    for (int64_t i = 0; i < cnt; ++i) {
      const F2kAuthValue *e = &flat[i * n_blocks];
      F2kAuthValue acc = bb->f2k_mul_const(e[0], coeff[0]);
      for (int64_t j = 1; j < n_blocks; ++j)
        acc = bb->f2k_add(acc, bb->f2k_mul_const(e[j], coeff[j]));
      out.push_back(acc);
    }
  }

  // ∏_i (r − elem_i) over the authenticated wires (r is a public constant; in
  // F(2^128) subtraction is XOR, so r − e = f2k_add_const(e, r)). Folds 4
  // factors per mul5_ (a degree-5 product proof, no fresh VOLE); the last
  // mul5_ fills its free slots with a public 1. So the proof never calls
  // f2k_mul, and a session that runs only permutation proofs never starts the
  // f2k VOLE.
  F2kAuthValue eval_(const std::vector<F2kAuthValue> &v, block r) {
    F2kAuthValue acc, one;
    bb->f2k_add_const(acc, v[0], r);
    bb->f2k_add_const(one, F2kAuthValue{zero_block, zero_block},
                      makeBlock(0, 1));
    for (size_t i = 1; i < v.size(); i += 4) {
      F2kAuthValue f[5] = {acc, one, one, one, one};
      for (size_t j = 0; j < 4 && i + j < v.size(); ++j)
        bb->f2k_add_const(f[j + 1], v[i + j], r);
      mul5_(acc, f);
    }
    return acc;
  }

  // Check p_A(r) == p_B(r) by opening the difference of the two product
  // wires as zero: the prover sends M_A + M_B and the verifier compares it
  // with K_A + K_B. The two differ by (p_A(r) + p_B(r))·Δ, so a false claim
  // passes only if the prover guesses that multiple of Δ. An honest message
  // equals K_A + K_B, which the verifier already holds, so it reveals
  // nothing.
  void open_eq_(const F2kAuthValue &pa, const F2kAuthValue &pb) {
    const block diff = pa.mac ^ pb.mac;
    if (bb->party == ALICE) {
      bb->io->send_data(&diff, sizeof(block));
      bb->io->flush();
    } else {
      block recv;
      bb->io->recv_data(&recv, sizeof(block));
      expecting(cmpBlock(&recv, &diff, 1) == 1, mismatch_.c_str());
    }
  }
};

// Pack a record — the concatenation (in order) of the authenticated bits of
// `parts` — into ⌈total/128⌉ f2k wires appended to `out`, the form ZKPermProof
// permutes. Each 128-bit block places component bit i at field position Xⁱ
// (GaloisFieldPacking computes Σ·Xⁱ), so two records with identical cleartext
// collapse to identical field values whether their bits were committed as
// public or private wires. The shared bit→f2k conversion for the
// permutation-based RAM/ROM/set data structures.
template <class V>
inline void ramzk_pack_record(ZKBoolBase *bb,
                              std::initializer_list<const V *> parts,
                              vector<F2kAuthValue> &out) {
  vector<block> wire;
  for (const V *p : parts) {
    std::vector<block> pb = int_blocks(*p);   // copy each wire's block out
    wire.insert(wire.end(), pb.begin(), pb.end());
  }
  const int64_t total = (int64_t)wire.size();
  const int64_t mblk = (total + 127) / 128;
  GaloisFieldPacking gfp;
  for (int64_t blk = 0; blk < mblk; ++blk) {
    const int64_t lo = blk * 128;
    const int64_t k = std::min<int64_t>(128, total - lo);
    // packing() consumes exactly 128 lanes; zero-pad the final short block.
    block blk128[128];
    bool bit128[128];                           // cleartext bits (ALICE)
    for (int i = 0; i < 128; ++i) {
      block b = (i < k) ? wire[lo + i] : zero_block;
      blk128[i] = b;
      bit128[i] = (i < k) && getLSB(b);
    }
    block mac, val;
    gfp.packing(&mac, blk128);                  // Σ wireᵢ·Xⁱ
    gfp.packing(&val, bit128);                  // Σ bitᵢ·Xⁱ
    out.push_back(bb->f2k_wire(val, mac));       // f2k_wire zeroes BOB's val
  }
}

// Bit width needed to hold any value in [0, hi]: the smallest b with 2^b > hi.
inline int64_t ramzk_bits_for(int64_t hi) {
  expecting(hi >= 0 && hi < (int64_t(1) << 62),
            "ramzk_bits_for: hi must be in [0, 2^62)");
  int64_t b = 1;
  while ((int64_t(1) << b) <= hi)
    ++b;
  return b;
}

} // namespace emp
#endif // EMP_ZK_PERM_PROOF_H__
