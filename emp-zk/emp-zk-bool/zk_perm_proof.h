#ifndef EMP_ZK_PERM_PROOF_H__
#define EMP_ZK_PERM_PROOF_H__

#include "emp-zk/emp-zk-bool/zk_session.h"
#include <array>
#include <memory>

namespace emp {
using namespace std;

// Degree-N polynomial-product proof over authenticated F(2^128) values — the
// high-fan-in multiply that powers the permutation product below. polyPrdtN
// commits the coefficients of P(Δ) = ∏ᵢ(kᵢ + xᵢΔ) so N field elements
// multiply for the cost of N VOLEs (the QuickSilver trick); batch_check folds
// all buffered products under one Fiat–Shamir uni-hash and opens a single MAC.
// Owned per ZKPermProof instance — its only consumer — and only allocated when
// a product actually runs.
class RamPolyPrdt {
public:
  int party;
  BoolIO *io;
  block delta[4];
  int64_t buffer_sz = 1 << 16;
  // ALICE: per-call coefficients of Δ⁰..Δ⁴; BOB: only [0] = poly(Δ).
  std::array<std::vector<block>, 5> buffer;
  Ferret *ferret = nullptr;
  int64_t num;

  RamPolyPrdt(int party, BoolIO *io, Ferret *ferret)
      : party(party), io(io), ferret(ferret) {
    if (party == ALICE) {
      for (auto &b : buffer)
        b.resize(buffer_sz);
    } else {
      buffer[0].resize(buffer_sz);
      delta[0] = ferret->Delta;
      gfmul(delta[0], delta[0], delta + 1);
      gfmul(delta[1], delta[0], delta + 2);
      gfmul(delta[2], delta[0], delta + 3);
    }
    num = 0;
  }

  ~RamPolyPrdt() { batch_check(); }

  void batch_check() {
    if (num == 0)
      return;
    io->flush();
    std::vector<block> chi(num);
    block check_sum[5];
    if (party == ALICE) {
      block seed = io->io->get_digest();
      uni_hash_coeff_gen(chi.data(), seed, num);

      for (int i = 0; i < 5; ++i)
        vector_inn_prdt_sum_red(check_sum + i, chi.data(), buffer[i].data(), num);

      // TODO mask
      //
      io->send_data(check_sum, 5 * sizeof(block));
      io->flush();
    } else {
      block seed = io->io->get_digest();
      uni_hash_coeff_gen(chi.data(), seed, num);

      block B;
      vector_inn_prdt_sum_red(&B, chi.data(), buffer[0].data(), num);

      // TODO mask
      //
      io->recv_data(check_sum, 5 * sizeof(block));

      block t[4];
      for (int i = 0; i < 4; ++i)
        gfmul(check_sum[i + 1], delta[i], &t[i]);
      check_sum[0] ^= (t[0] ^ t[1] ^ t[2] ^ t[3]);
      expecting(memcmp(&B, check_sum, 16) == 0, "product by polynomial fails");
    }
    num = 0;
  }

  // Templated polynomial-product MAC step.
  //
  // x[0..N-1], m[0..N-1] are the prover's (cleartext, MAC) pairs for N
  // committed values; m_last is the MAC the prover commits as the
  // product. Both sides build coefficients of the polynomial-in-Δ
  //
  //     P(Δ) = (k_1 + x_1 Δ)(k_2 + x_2 Δ) … (k_N + x_N Δ)
  //
  // (with k_i ≡ m_i in this code's spelling). The Δ^N coefficient is
  // x_1·x_2·…·x_N (the cleartext product, available to the caller as
  // `v`); we buffer the lower coefficients 0..N-1. The Δ^(N-1) slot
  // also folds in the m_last correction term so that
  //   buffer_{N-1} − m_last = coefficient of Δ^(N-1).
  // Higher-index buffers (N..4) are zeroed so batch_check's later
  // chi-fold sees a clean degree-N polynomial regardless of N.
  //
  // ALICE expands the polynomial iteratively
  //   poly_{i+1}[j] = poly_i[j] · k_{i+1} + poly_i[j-1] · x_{i+1}.
  // BOB stores only B = ⊓_i m_i + m_last · Δ^(N-1) in buffer0.
  template <int N>
  inline void polyPrdtN(const block *x, const block *m, const block &m_last) {
    static_assert(N >= 3 && N <= 5);
    if (num >= buffer_sz) batch_check();

    if (party == ALICE) {
      block poly[N + 1] = {};
      block tmp;

      // Hand-expand the 2-term init: poly = (k_1 + x_1 Δ)(k_2 + x_2 Δ).
      gfmul(m[0], m[1], &poly[0]);
      gfmul(m[0], x[1], &poly[1]);
      gfmul(x[0], m[1], &tmp);
      poly[1] ^= tmp;
      gfmul(x[0], x[1], &poly[2]);

      // Iteratively multiply in (k_i + x_i Δ) for i = 2 .. N-1.
      for (int i = 2; i < N; ++i) {
        block new_poly[N + 1] = {};
        // At the LAST step (i == N-1) the new top coefficient
        // new_poly[N] = poly[N-1] · x_i would be the all-x term =
        // x_1·…·x_N = v, which the caller already has; skip it.
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

      // Coefficient layout: poly[0..N-2] → Δ⁰..Δ^(N-2); poly[N-1] folds in
      // m_last at Δ^(N-1); buffers N..4 zeroed so batch_check sees a clean
      // degree-N polynomial regardless of N.
      for (int j = 0; j < 5; ++j)
        buffer[j][num] = (j < N - 1)  ? poly[j]
                       : (j == N - 1) ? (poly[N - 1] ^ m_last)
                                      : zero_block;
    } else {
      block prod = m[0];
      for (int i = 1; i < N; ++i) gfmul(prod, m[i], &prod);
      // delta[k] = Δ^(k+1) (delta[0]=Δ, delta[1]=Δ², …);
      // delta[N-2] = Δ^(N-1).
      block adj;
      gfmul(m_last, delta[N - 2], &adj);
      buffer[0][num] = prod ^ adj;
    }
    num++;
  }
};

// Zero-knowledge multiset permutation check over authenticated elements —
// the core gadget for VOLE-ZK RAM/ROM (Yang–Heath, "Two Shuffles Make a
// RAM", §2.2). The prover commits two multisets A and B; the proof passes
// iff A is a permutation of B.
//
// Method: encode each multiset as a polynomial with its elements as roots,
//   p_A(X) = ∏_i (X − a_i),   p_B(X) = ∏_i (X − b_i),
// and test p_A = p_B at a public-coin challenge r by Schwartz–Zippel. Both
// products are evaluated in zero knowledge as a chain of authenticated f2k
// multiplications, batched through a degree-N product proof (mul_poly_, the
// QuickSilver high-fan-in trick — so most factors cost no fresh VOLE); the
// two product wires are then opened and compared.
//
// Payload size (bits) is fixed at construction. ≤128 bits → each element is
// one F(2^128) wire and the product runs directly. Larger → an element spans
// m = ⌈bits/128⌉ wires that are *compressed* to one wire by the random linear
// combination Σ_j c_j·B_j with a public-coin coefficient vector c (a local
// op — f2k_mul_const / f2k_add — no VOLE, no comms). Distinct tuples collide
// under a random c only with prob ≈ 1/2^128, after which the single-wire
// product applies. Total soundness ≈ (|A| + m)/2^128.
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
class ZKPermProof {
public:
  ZKBoolBase *bb;
  int64_t n_blocks;   // wires per element = ⌈bits/128⌉

  // Pending raw elements (n_blocks wires each), awaiting compression. For
  // n_blocks == 1 add_* writes straight to the compressed lists and these
  // stay empty.
  std::vector<F2kAuthValue> a_pending, b_pending;
  // Collapsed single-wire elements; the permutation product runs over these.
  std::vector<F2kAuthValue> a_elems_compressed, b_elems_compressed;

  // Degree-N product engine for eval_'s batched factors. Lazily created on
  // first product, batch-checked in check_eq.
  std::unique_ptr<RamPolyPrdt> polyprdt;

  explicit ZKPermProof(ZKBoolSession &sess, int64_t payload_bits = 128)
      : bb(&sess.engine()), n_blocks((payload_bits + 127) / 128) {}

  // Append one element. The pointer form reads n_blocks consecutive wires;
  // the reference form is a convenience for single-wire payloads (≤128 bits).
  void add_A(const F2kAuthValue *e) {
    if (n_blocks == 1) a_elems_compressed.push_back(*e);
    else a_pending.insert(a_pending.end(), e, e + n_blocks);
  }
  void add_B(const F2kAuthValue *e) {
    if (n_blocks == 1) b_elems_compressed.push_back(*e);
    else b_pending.insert(b_pending.end(), e, e + n_blocks);
  }
  void add_A(const F2kAuthValue &e) { add_A(&e); }   // requires n_blocks == 1
  void add_B(const F2kAuthValue &e) { add_B(&e); }   // requires n_blocks == 1

  // Fold the pending multi-wire elements into the compressed lists, drawing a
  // fresh coefficient vector by Fiat–Shamir now (so it binds the committed
  // elements being folded). No-op for single-wire payloads or an empty
  // buffer. Matched elements must be folded in the same call (see class doc).
  void compress() {
    if (n_blocks == 1 || (a_pending.empty() && b_pending.empty())) return;
    bb->io->flush();
    block seed = bb->io->io->get_digest();
    std::vector<block> coeff(n_blocks);
    PRG(&seed).random_block(coeff.data(), (int)n_blocks);
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
      // Domain-separated from any compress() coefficient seed so r stays
      // independent even when the last fold and this draw share a transcript
      // point (compress() is local — it does no IO).
      block seed = bb->io->io->get_digest() ^ makeBlock(0, 1);
      block r;
      PRG(&seed).random_block(&r, 1);
      F2kAuthValue pa = eval_(a_elems_compressed, r);
      F2kAuthValue pb = eval_(b_elems_compressed, r);
      open_eq_(pa, pb);
      if (polyprdt) polyprdt->batch_check();   // verify the batched products
    }
    a_pending.clear();
    b_pending.clear();
    a_elems_compressed.clear();
    b_elems_compressed.clear();
  }

private:
  // Degree-N product of f2k wires: out = ∏ inputs, MAC committed through the
  // owned polyprdt's degree-N check (lazily created). ALICE's f2k_mul_v gives
  // the cleartext product, BOB's gives zero; f2k_pack_v puts it in MAC layout.
  // Both f2k_mul_v and f2k_pack_v are backend primitives (shared Δ/Ferret).
  template <typename... Args>
  void mul_poly_(F2kAuthValue &out, Args... args) {
    constexpr int N = sizeof...(args);
    static_assert(N >= 3 && N <= 5, "mul_poly_ supports N=3, 4, 5");
    if (!polyprdt)
      polyprdt = std::make_unique<RamPolyPrdt>(bb->party, bb->io, bb->ferret);

    const F2kAuthValue in[N] = { args... };
    block vals[N], macs[N];
    for (int i = 0; i < N; ++i) {
      vals[i] = in[i].val;
      macs[i] = in[i].mac;
    }
    block v = bb->f2k_mul_v(N, vals);
    block m = bb->f2k_pack_v(v);
    polyprdt->template polyPrdtN<N>(vals, macs, m);
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
  // factors per mul_poly_ (a degree-5 polynomial proof, no fresh VOLE) and
  // finishes the remainder with plain f2k_mul.
  F2kAuthValue eval_(const std::vector<F2kAuthValue> &v, block r) {
    F2kAuthValue acc;
    bb->f2k_add_const(acc, v[0], r);
    size_t i = 1;
    for (; i + 3 < v.size(); i += 4) {
      F2kAuthValue x[4];
      for (int j = 0; j < 4; ++j)
        bb->f2k_add_const(x[j], v[i + j], r);
      mul_poly_(acc, acc, x[0], x[1], x[2], x[3]);
    }
    for (; i < v.size(); ++i) {
      F2kAuthValue x;
      bb->f2k_add_const(x, v[i], r);
      bb->f2k_mul(acc, acc, x);
    }
    return acc;
  }

  // Open both product wires and check p_A(r) == p_B(r): the prover ships her
  // two MAC shares; the verifier folds them into its keys, recovering
  // val_A·Δ and val_B·Δ, and checks they match (⇒ val_A == val_B). Forging a
  // pass with val_A ≠ val_B would require guessing Δ.
  void open_eq_(const F2kAuthValue &pa, const F2kAuthValue &pb) {
    block macs[2] = { pa.mac, pb.mac };
    if (bb->party == ALICE) {
      bb->io->send_data(macs, 2 * sizeof(block));
      bb->io->flush();
    } else {
      block recv[2];
      bb->io->recv_data(recv, 2 * sizeof(block));
      macs[0] = macs[0] ^ recv[0];
      macs[1] = macs[1] ^ recv[1];
      expecting(memcmp(macs, macs + 1, sizeof(block)) == 0,
                "ZKPermProof: A is not a permutation of B");
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
  int64_t b = 1;
  while ((int64_t(1) << b) <= hi)
    ++b;
  return b;
}

} // namespace emp
#endif // EMP_ZK_PERM_PROOF_H__
