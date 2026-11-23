#ifndef EMP_ZK_BOOL_VERIFIER_H__
#define EMP_ZK_BOOL_VERIFIER_H__

// The verifier (BOB) side of the proof engine: the BOB branches of the
// authenticated-triple protocol. Included from zk_bool_base.h.

namespace emp {
using namespace std;

class ZKBoolVerifier : public ZKBoolBase {
public:
  // The verifier holds delta (and zdelta = delta ^ 1 for negation). Both
  // are also reachable via ZKBoolBase::delta; zdelta is verifier-
  // specific because NOT folds it on the wire.
  block zdelta;

  ZKBoolVerifier(BoolIO *io, int64_t expected_cots = 0)
      : ZKBoolBase(BOB, io, expected_cots) {
    zdelta = delta ^ bit0_mask;
    // PUBLIC label for bit 1: xor zdelta in so the wire MAC is right
    // even though there's no cleartext flip.
    pub_label[1] = pub_label[1] ^ zdelta;
  }

  ~ZKBoolVerifier() override {
    // Flush any leftover f2k batch first (see ZKBoolProver dtor).
    if (f2k_ready && f2k_check_cnt != 0)
      f2k_check_manage();

    if (check_cnt != 0)
      andgate_correctness_check_manage();

    // finalize_macs (VER side): hash the wire MACs we computed for revealed
    // outputs, compare against the prover's digest. Mismatch → cheat.
    char digest[Hash::DIGEST_SIZE];
    auth_hash.digest(digest);
    char digest2[Hash::DIGEST_SIZE];
    io->recv_data(digest2, Hash::DIGEST_SIZE);
    expecting(memcmp(digest, digest2, Hash::DIGEST_SIZE) == 0,
              "emp-zk-bool finalize");
  }

  // ---- Engine gate / I-O surface (verifier) ------------------------------

  block not_block(block in) override {
    // NOT folds zdelta so the MAC stays consistent under negation.
    return in ^ zdelta;
  }

  block and_block(block l, block r) override {
    ++gid;
    return auth_compute_and(l, r);
  }

  void feed_bits(block *out, int from_party, const bool *in, size_t n) override {
    // Guard the raw primitive too (engine() is public): an invalid owner would
    // leave the prover on the other branch and desync the transcript.
    expecting(from_party == ALICE || from_party == PUBLIC,
              "ZKBoolVerifier::feed_bits: input owner must be ALICE or PUBLIC");
    if (from_party == ALICE)
      authenticated_bits_input(out, in, static_cast<int64_t>(n));
    else
      for (size_t i = 0; i < n; ++i) out[i] = pub_label[in[i]];
  }

  void reveal_bits(bool *out, int to_party, const block *in, size_t n) override {
    expecting(to_party == ALICE || to_party == BOB || to_party == PUBLIC,
              "ZKBoolVerifier::reveal_bits: recipient must be ALICE, BOB, or PUBLIC");
    if (to_party == BOB || to_party == PUBLIC)
      verify_output(out, in, static_cast<int64_t>(n));
  }

private:
  // ---- Verifier-side protocol methods ------------------------------------

  // Authenticated-bit input: receive the prover's masking flips, fold them
  // into the COT keys to recover the per-bit MAC structure.
  void authenticated_bits_input(block *auth, const bool *in, int64_t len) {
    ferret->next_n(auth, len);
    for (int64_t i = 0; i < len; ++i) {
      bool buff = io->recv_bit();
      auth[i] = clear_lsb(xor_delta_if(auth[i], buff));
    }
  }

  // Authenticated AND: receive the prover's masking bit, fold it into the fresh
  // COT key to reconstruct the wire key. The COT is drawn one at a time from the
  // SilentFerret streaming interface (wire-free); the wire key is buffered
  // alongside the inputs for the eventual batch check. When the buffer fills,
  // run the check.
  block auth_compute_and(block a, block b) {
    if (check_cnt == CHECK_SZ) {
      andgate_correctness_check_manage();
      check_cnt = 0;
    }

    block auth;
    ferret->next_n(&auth, 1);                     // fresh COT from the stream
    andgate_left_buffer[check_cnt]  = a;
    andgate_right_buffer[check_cnt] = b;

    bool d = io->recv_bit();
    auth = clear_lsb(xor_delta_if(auth, d));

    andgate_out_buffer[check_cnt] = auth;          // wire key for the check
    check_cnt++;
    return auth;
  }

  // Output reveal: receive each claimed cleartext bit, recompute the
  // expected MAC under our own delta, drop into the auth_hash transcript.
  // The destructor compares this digest against the prover's.
  void verify_output(bool *b, const block *output, int64_t length) {
    for (int64_t i = 0; i < length; ++i)
      b[i] = io->recv_bit();
    if ((int64_t)auth_tmp.size() < length)
      auth_tmp.resize(length);
    for (int64_t i = 0; i < length; ++i)
      auth_tmp[i] = xor_delta_if(output[i], b[i]);
    auth_hash.put_block(auth_tmp.data(), length);
  }

  // ---- Batch-check hooks (called from the base skeleton) ----------------

  void andgate_correctness_check(block *ret, int64_t task_n,
                                 block chi_seed) override {
    if (task_n == 0) return;
    const block *left    = andgate_left_buffer.data();
    const block *right   = andgate_right_buffer.data();
    const block *gateout = andgate_out_buffer.data();

    // Chunked fold (mirror of the prover): per cache-resident chunk form
    // B = left*right + gateout*delta, accumulate the unreduced 256-bit chi
    // inner product across chunks, and reduce once at the end. Avoids the
    // multi-MB buffer rewrite and a task_n-sized chi array.
    PRG prg(&chi_seed);
    constexpr int64_t kChunk = 1024;
    block chi[kChunk], B[kChunk];
    block acc[2] = {zero_block, zero_block};
    for (int64_t base = 0; base < task_n; base += kChunk) {
      const int64_t m = (task_n - base < kChunk) ? (task_n - base) : kChunk;
      prg.random_block(chi, m);
      for (int64_t i = 0; i < m; ++i) {
        block bb, tmp;
        gfmul(left[base + i], right[base + i], &bb);
        gfmul(gateout[base + i], delta, &tmp);
        B[i] = bb ^ tmp;
      }
      block p[2];
      vector_inn_prdt_sum_no_red(p, chi, B, m);
      acc[0] = acc[0] ^ p[0];  acc[1] = acc[1] ^ p[1];
    }
    ret[0] = reduce(acc[0], acc[1]);
  }

  void andgate_correctness_aggregate(block *sum) override {
    block ope_data[128];
    ferret->next_n(ope_data, 128);
    block B_star;
    pack.packing(&B_star, ope_data);
    B_star = B_star ^ sum[0];
    block A_star[2];
    io->recv_data(A_star, 2 * sizeof(block));
    block W;
    gfmul(A_star[1], delta, &W);
    W = W ^ A_star[0];
    expecting(cmpBlock(&W, &B_star, 1) == 1, "emp_zk_bool AND batch check");
  }

  // ---- f2k wire ops (BOB) ---------------------------------------------

  void f2k_add_const(F2kAuthValue &out, const F2kAuthValue &in,
                     block c) override {
    block d;
    gfmul(delta, c, &d);   // verifier folds Δ·c into the mac
    out.mac = in.mac ^ d;
    out.val = zero_block;  // verifier has no cleartext
  }

  void f2k_mul(F2kAuthValue &out, const F2kAuthValue &a,
               const F2kAuthValue &b) override {
    f2k_init();
    if (f2k_check_cnt == f2k_buffer_sz) {
      f2k_check_manage();
      f2k_check_cnt = 0;
    }
    if (f2k_authval_cnt == f2k_buffer_sz)
      f2k_pre_buffer_refill();
    f2k_left_mac[f2k_check_cnt] = a.mac;
    f2k_rght_mac[f2k_check_cnt] = b.mac;

    // Receive the prover's masking difference, fold Δ·d into the pre-drawn
    // VOLE key to recover the wire key for this gate output.
    block d;
    io->recv_data(&d, sizeof(block));
    gfmul(d, delta, &d);
    f2k_auth_buffer[f2k_authval_cnt].mac ^= d;
    out.val = zero_block;
    out.mac = f2k_auth_buffer[f2k_authval_cnt].mac;
    f2k_check_cnt++;
    f2k_authval_cnt++;
  }

  // Verifier has no cleartext, so the polynomial product's top coefficient
  // is zero.
  block f2k_mul_v(int64_t, const block *) override { return zero_block; }

  void f2k_check_manage() override {
    io->flush();
    block seed = check_seed(kF2kCheckDomain);
    block sum[2] = { zero_block, zero_block };
    f2k_check(sum, f2k_check_cnt, seed);

    block ope_data[128];
    ferret->next_n(ope_data, 128);
    block B_star;
    pack.packing(&B_star, ope_data);
    B_star = B_star ^ sum[0];
    block A_star[2];
    io->recv_data(A_star, 2 * sizeof(block));
    block W;
    gfmul(A_star[1], delta, &W);
    W = W ^ A_star[0];
    expecting(cmpBlock(&W, &B_star, 1) == 1, "emp-zk-bool f2k mult batch check");
    io->flush();
  }

 private:
  // Verifier check coefficient B = lmac·rmac + outmac·Δ, chi-reduced one
  // chunk at a time, as the AND-gate check does.
  void f2k_check(block *ret, int64_t task_n, block chi_seed) {
    if (task_n == 0) return;
    const block *lmac = f2k_left_mac.data(), *rmac = f2k_rght_mac.data();
    const AuthValueF2k *out =
        f2k_auth_buffer.data() + (f2k_authval_cnt - f2k_check_cnt);
    PRG prg(&chi_seed);
    constexpr int64_t kChunk = 1024;
    block chi[kChunk], B[kChunk];
    block acc[2] = {zero_block, zero_block};
    for (int64_t base = 0; base < task_n; base += kChunk) {
      const int64_t m = (task_n - base < kChunk) ? (task_n - base) : kChunk;
      prg.random_block(chi, m);
      for (int64_t i = 0; i < m; ++i) {
        const int64_t j = base + i;
        block tmp;
        gfmul(lmac[j], rmac[j], &B[i]);
        gfmul(out[j].mac, delta, &tmp);
        B[i] = B[i] ^ tmp;
      }
      block p[2];
      vector_inn_prdt_sum_no_red(p, chi, B, m);
      acc[0] = acc[0] ^ p[0];  acc[1] = acc[1] ^ p[1];
    }
    ret[0] = reduce(acc[0], acc[1]);
  }

 public:
};

} // namespace emp
#endif
