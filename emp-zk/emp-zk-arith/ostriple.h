#ifndef FP_OS_TRIPLE_H__
#define FP_OS_TRIPLE_H__

#include "emp-ot/emp-ot.h"
#include "emp-zk/emp-zk-arith/triple_auth.h"

namespace emp {
using namespace std;

// Bit-position helpers (layout-agnostic).
#define LOW64(x) _mm_extract_epi64((block)x, 0)
#define HIGH64(x) _mm_extract_epi64((block)x, 1)

// AuthValue accessors (val-first layout: val in low 64, mac in high 64).
// Use these for AuthValue casts so the layout intent is explicit; keep
// LOW64/HIGH64 for raw block bit extraction (chi_seed, F_p deltas, etc.).
#define VAL(x) _mm_extract_epi64((block)x, 0)
#define MAC(x) _mm_extract_epi64((block)x, 1)
// Construct an AuthValue-bytes __uint128_t from (val, mac).
#define MAKE_AUTH(val_, mac_) \
    ((__uint128_t)makeBlock((uint64_t)(mac_), (uint64_t)(val_)))

class FpOSTriple {
public:
  int party;
  int64_t triple_n;
  __uint128_t delta;

  int64_t check_cnt = 0;
  std::vector<__uint128_t> andgate_out_buffer;
  std::vector<__uint128_t> andgate_left_buffer;
  std::vector<__uint128_t> andgate_right_buffer;

  BoolIO *io;
  PRG prg;
  FpVOLE<AuthValueFp> *vole = nullptr;
  FpAuthHelper *auth_helper = nullptr;

  int64_t CHECK_SZ = 1024 * 1024;

  FpOSTriple(int party, BoolIO *io) {
    this->party = party;
    this->io = io;

    if (party == BOB) delta_gen();
    vole = new FpVOLE<AuthValueFp>(3 - party, io);
    if (party == BOB) vole->set_delta((uint64_t)delta);
    // One persistent sVOLE session for the whole proof; authenticated
    // values are drawn via vole->next_n() (the EdaBits / FpPolyProof
    // gadgets borrow this same vole). Amortizes the per-round end-work
    // instead of paying it per draw as repeated run(_, 1) did. Closed in
    // the destructor, after those borrowers have been torn down.
    vole->begin();

    andgate_out_buffer.resize(CHECK_SZ);
    andgate_left_buffer.resize(CHECK_SZ);
    andgate_right_buffer.resize(CHECK_SZ);
    __uint128_t tmp;
    vole->next_n((AuthValueFp *)&tmp, 1);
    // Pre-draw the first batch of authenticated values into
    // andgate_out_buffer; each multiplication consumes one slot (reads the
    // fresh VOLE, then overwrites it with the gate's output MAC for the
    // batch check), and the batch boundary reloads after each check. The
    // bulk draw keeps the VOLE recv contiguous instead of interleaving a
    // single next_n with the per-gate value exchange the other way.
    vole->next_n((AuthValueFp *)andgate_out_buffer.data(), CHECK_SZ);

    auth_helper = new FpAuthHelper(party, io);
  }

  ~FpOSTriple() {
    if (check_cnt != 0)
      andgate_correctness_check_manage();
    auth_helper->flush();
    delete auth_helper;
    vole->end();        // close the persistent sVOLE session (borrowers already gone)
    delete vole;
  }
  /* ---------------------inputs----------------------*/

  /*
   * authenticated bits for inputs of the prover
   */
  __uint128_t authenticated_val_input(uint64_t w) {
    __uint128_t mac;
    vole->next_n((AuthValueFp *)&mac, 1);

    uint64_t lam = PR - w;
    lam = add_mod(VAL(mac), lam);
    io->send_data(&lam, sizeof(uint64_t));
    return MAKE_AUTH(w, MAC(mac));
  }

  void authenticated_val_input(__uint128_t *label, const uint64_t *w, int64_t len) {
    std::vector<uint64_t> lam(len);
    vole->next_n((AuthValueFp *)label, len);

    for (int64_t i = 0; i < len; ++i) {
      lam[i] = PR - w[i];
      lam[i] = add_mod(VAL(label[i]), lam[i]);
      label[i] = MAKE_AUTH(w[i], MAC(label[i]));
    }
    io->send_data(lam.data(), len * sizeof(uint64_t));
  }

  __uint128_t authenticated_val_input() {
    __uint128_t key;
    vole->next_n((AuthValueFp *)&key, 1);

    uint64_t lam;
    io->recv_data(&lam, sizeof(uint64_t));

    uint64_t delta_lam = mult_mod(lam, (uint64_t)delta);
    // BOB's key has val=0 in low, mac in high (val-first); only the mac
    // side picks up the correction.
    return MAKE_AUTH(0, add_mod(MAC(key), delta_lam));
  }

  void authenticated_val_input(__uint128_t *label, int64_t len) {
    std::vector<uint64_t> lam(len);
    vole->next_n((AuthValueFp *)label, len);

    io->recv_data(lam.data(), len * sizeof(uint64_t));

    for (int64_t i = 0; i < len; ++i) {
      uint64_t delta_lam = mult_mod(lam[i], (uint64_t)delta);
      label[i] = MAKE_AUTH(0, add_mod(MAC(label[i]), delta_lam));
    }
  }

  /*
   * authenticated bits for computing AND gates
   */
  __uint128_t auth_compute_mul_send(__uint128_t Ma, __uint128_t Mb) {
    if (check_cnt == CHECK_SZ) {
      andgate_correctness_check_manage();
      check_cnt = 0;
      vole->next_n((AuthValueFp *)andgate_out_buffer.data(), CHECK_SZ);
    }
    __uint128_t mac = andgate_out_buffer[check_cnt];   // pre-drawn fresh VOLE
    andgate_left_buffer[check_cnt] = Ma;
    andgate_right_buffer[check_cnt] = Mb;

    uint64_t d = mult_mod(VAL(Ma), VAL(Mb));
    uint64_t s = PR - d;
    s = add_mod(VAL(mac), s);
    io->send_data(&s, sizeof(uint64_t));

    mac = MAKE_AUTH(d, MAC(mac));
    andgate_out_buffer[check_cnt] = mac;                // overwrite with output MAC
    check_cnt++;

    return mac;
  }

  __uint128_t auth_compute_mul_recv(__uint128_t Ka, __uint128_t Kb) {
    if (check_cnt == CHECK_SZ) {
      andgate_correctness_check_manage();
      check_cnt = 0;
      vole->next_n((AuthValueFp *)andgate_out_buffer.data(), CHECK_SZ);
    }
    __uint128_t key = andgate_out_buffer[check_cnt];   // pre-drawn fresh VOLE key
    andgate_left_buffer[check_cnt] = Ka;
    andgate_right_buffer[check_cnt] = Kb;

    uint64_t d;
    io->recv_data(&d, sizeof(uint64_t));
    uint64_t delta_d = mult_mod(d, (uint64_t)delta);
    // BOB's key: val=0 in low, mac in high (val-first); apply
    // correction to the mac side only.
    key = MAKE_AUTH(0, add_mod(MAC(key), delta_d));

    andgate_out_buffer[check_cnt] = key;                // overwrite with wire key
    check_cnt++;
    return key;
  }

  /* ---------------------check----------------------*/

  void andgate_correctness_check_manage() {
    io->flush();

    uint64_t U = 0, V = 0, W = 0;
    block share_seed;
    share_seed_gen(&share_seed, 1);
    io->flush();

    uint64_t sum[2] = {0, 0};
    andgate_correctness_check(sum, 0, 0, check_cnt, &share_seed);
    if (party == ALICE) {
      U = sum[0];
      V = sum[1];
    } else
      W = sum[0];

    if (party == ALICE) {
      __uint128_t ope_data;
      vole->next_n((AuthValueFp *)&ope_data, 1);
      uint64_t A0_star = MAC(ope_data);
      uint64_t A1_star = VAL(ope_data);
      uint64_t check_sum[2];
      check_sum[0] = add_mod(U, A0_star);
      check_sum[1] = add_mod(V, A1_star);
      io->send_data(check_sum, 2 * sizeof(uint64_t));
    } else {
      __uint128_t ope_data;
      vole->next_n((AuthValueFp *)&ope_data, 1);
      uint64_t B_star = MAC(ope_data);
      W = add_mod(W, B_star);
      uint64_t check_sum[2];
      io->recv_data(check_sum, 2 * sizeof(uint64_t));
      check_sum[1] = mult_mod(check_sum[1], delta);
      check_sum[1] = add_mod(check_sum[1], W);
      expecting(check_sum[0] == check_sum[1], "multiplication gates check fails");
    }
    io->flush();
  }

  void andgate_correctness_check(uint64_t *ret, int thr_idx, uint32_t start,
                                 uint32_t task_n, block *chi_seed) {
    if (task_n == 0)
      return;
    __uint128_t *left = andgate_left_buffer.data();
    __uint128_t *right = andgate_right_buffer.data();
    __uint128_t *gateout = andgate_out_buffer.data();

    std::vector<uint64_t> chi(task_n);
    uint64_t seed = mod(LOW64(chi_seed[thr_idx]));
    uni_hash_coeff_gen(chi.data(), seed, task_n);
    if (party == ALICE) {
      uint64_t A0, A1;
      uint64_t U = 0, V = 0;
      uint64_t a, b, ma, mb, mc;
      for (uint32_t i = start, k = 0; i < start + task_n; ++i, ++k) {
        a = VAL(left[i]);
        ma = MAC(left[i]);
        b = VAL(right[i]);
        mb = MAC(right[i]);
        mc = MAC(gateout[i]);
        A0 = mult_mod(ma, mb);
        A1 = add_mod(mult_mod(a, mb), mult_mod(b, ma));
        uint64_t tmp = PR - mc;
        A1 = add_mod(A1, tmp);
        U = add_mod(U, mult_mod(A0, chi[k]));
        V = add_mod(V, mult_mod(A1, chi[k]));
      }
      ret[2 * thr_idx] = U;
      ret[2 * thr_idx + 1] = V;
    } else {
      uint64_t B;
      uint64_t W = 0;
      uint64_t ka, kb, kc;
      for (uint32_t i = start, k = 0; i < start + task_n; ++i, ++k) {
        ka = MAC(left[i]);
        kb = MAC(right[i]);
        kc = MAC(gateout[i]);
        B = add_mod(mult_mod(ka, kb), mult_mod(kc, delta));
        W = add_mod(W, mult_mod(B, chi[k]));
      }
      ret[thr_idx] = W;
    }
  }

  /*
   * verify the output
   * open and check if the value equals 1
   */
  void reveal_send(const __uint128_t *output, uint64_t *value, int64_t len) {
    for (int64_t i = 0; i < len; ++i) {
      value[i] = VAL(output[i]);
      uint64_t mac = MAC(output[i]);
      auth_helper->store(mac); // TODO
    }
    io->send_data(value, len * sizeof(uint64_t));
  }

  void reveal_recv(const __uint128_t *output, uint64_t *value, int64_t len) {
    io->recv_data(value, len * sizeof(uint64_t));
    for (int64_t i = 0; i < len; ++i) {
      uint64_t mac = mult_mod(value[i], LOW64(delta));
      mac = add_mod(mac, MAC(output[i]));
      auth_helper->store(mac); // TODO
    }
  }

  void reveal_check_send(const __uint128_t *output, const uint64_t *value,
                         int64_t len) {
    std::vector<uint64_t> val_real(len);
    reveal_send(output, val_real.data(), len);
  }

  void reveal_check_recv(const __uint128_t *output, const uint64_t *val_exp,
                         int64_t len) {
    std::vector<uint64_t> val_real(len);
    reveal_recv(output, val_real.data(), len);
    expecting(memcmp(val_exp, val_real.data(), len * sizeof(uint64_t)) == 0,
              "arithmetic reveal value not expected");
  }

  void reveal_check_zero(const __uint128_t *output, int64_t len) {
    for (int64_t i = 0; i < len; ++i) {
      uint64_t mac = MAC(output[i]);
      auth_helper->store(mac);
    }
  }

  /* ---------------------helper functions----------------------*/

  void delta_gen() {
    PRG prg;
    prg.random_data(&delta, sizeof(__uint128_t));
    extract_fp(delta);
  }

  void share_seed_gen(block *seed, uint32_t num) {
    block seed0;
    if (party == ALICE) {
      io->recv_data(&seed0, sizeof(block));
      PRG(&seed0).random_block(seed, num);
    } else {
      prg.random_block(&seed0, 1);
      io->send_data(&seed0, sizeof(block));
      PRG(&seed0).random_block(seed, num);
    }
  }

  // sender
  void refill_send(__uint128_t *yz, int64_t *cnt, int64_t sz) {
    vole->next_n((AuthValueFp *)yz, sz);
    *cnt = 0;
  }

  // recver
  void refill_recv(__uint128_t *yz, int64_t *cnt, int64_t sz) {
    vole->next_n((AuthValueFp *)yz, sz);
    *cnt = 0;
  }

  void compute_mu_prv(__uint128_t &ret, __uint128_t z1, __uint128_t *triple,
                      __uint128_t epsilon, __uint128_t sigma) {
    __uint128_t tmp1 = auth_mac_subtract(triple[2], z1);
    __uint128_t tmp2 = auth_mac_mul_const(triple[0], VAL(sigma));
    __uint128_t tmp3 = auth_mac_mul_const(triple[1], VAL(epsilon));
    __uint128_t tmp4 = mult_mod(VAL(epsilon), VAL(sigma));
    tmp1 = auth_mac_add(tmp1, tmp2);
    tmp1 = auth_mac_add(tmp1, tmp3);
    ret = auth_mac_add_const(tmp1, tmp4);
  }
  void compute_mu_vrf(__uint128_t &ret, __uint128_t z1, __uint128_t *triple,
                      __uint128_t epsilon, __uint128_t sigma) {
    __uint128_t tmp1 = auth_key_subtract(triple[2], z1);
    __uint128_t tmp2 = auth_key_mul_const(triple[0], sigma);
    __uint128_t tmp3 = auth_key_mul_const(triple[1], epsilon);
    __uint128_t tmp4 = mod(epsilon * sigma, pr);
    tmp1 = auth_key_add(tmp1, tmp2);
    tmp1 = auth_key_add(tmp1, tmp3);
    ret = auth_key_add_const(tmp1, tmp4);
  }

  __uint128_t compute_mu_prv_opt(__uint128_t la, __uint128_t lb,
                                 __uint128_t eta_wr, __uint128_t *triple) {
    __uint128_t tmp1 = auth_mac_subtract(triple[2], eta_wr);
    __uint128_t tmp2 = auth_mac_mul_const(triple[0], VAL(lb));
    __uint128_t tmp3 = auth_mac_mul_const(triple[1], VAL(la));
    __uint128_t tmp4 = mult_mod(VAL(la), VAL(lb));
    tmp1 = auth_mac_add(tmp1, tmp2);
    tmp1 = auth_mac_add(tmp1, tmp3);
    return auth_mac_add_const(tmp1, tmp4);
  }

  __uint128_t compute_mu_vrf_opt(__uint128_t la, __uint128_t lb,
                                 __uint128_t eta_wr, __uint128_t *triple) {
    __uint128_t tmp1 = auth_key_subtract(triple[2], eta_wr);
    __uint128_t tmp2 = auth_key_mul_const(triple[0], lb);
    __uint128_t tmp3 = auth_key_mul_const(triple[1], la);
    __uint128_t tmp4 = mult_mod((uint64_t)la, (uint64_t)lb);
    tmp1 = auth_key_add(tmp1, tmp2);
    tmp1 = auth_key_add(tmp1, tmp3);
    return auth_key_add_const(tmp1, tmp4);
  }

  // prover: add 2 IT-MACs
  // return: [a] + [b]
  __uint128_t auth_mac_add(__uint128_t a, __uint128_t b) {
    block res = _mm_add_epi64((block)a, (block)b);
    return (__uint128_t)vec_mod(res);
  }

  // prover: add a IT-MAC with a constant
  // return: [a] + c (adds c into the val lane only; mac unchanged)
  __uint128_t auth_mac_add_const(__uint128_t a, __uint128_t c) {
    block cc = makeBlock(0, c);   // (high=0=mac-delta, low=c=val-delta)
    cc = _mm_add_epi64((block)a, cc);
    return (__uint128_t)vec_mod(cc);
  }

  // prover: subtract 2 IT-MACs
  // return: [a] - [b]
  __uint128_t auth_mac_subtract(__uint128_t a, __uint128_t b) {
    block res = _mm_sub_epi64(PRs, (block)b);
    res = _mm_add_epi64((block)a, res);
    return (__uint128_t)vec_mod(res);
  }

  // prover: multiplies IT-MAC with a constatnt
  // return: c*[a]
  __uint128_t auth_mac_mul_const(__uint128_t a, uint64_t c) {
    return (__uint128_t)mult_mod((block)a, c);
  }

  // verifier: add 2 IT-MACs (mac-only on BOB; val=0 throughout)
  // return: [a] + [b]
  __uint128_t auth_key_add(__uint128_t a, __uint128_t b) {
    return MAKE_AUTH(0, add_mod(MAC(a), MAC(b)));
  }

  // verifier: add an IT-MAC with a constant — subtract Δ·c from the mac
  // (BOB's view of "adding c to the IT-MAC of value v" is to adjust
  // his mac by -Δ·c so that K = mac' + Δ·(v+c) = original K).
  __uint128_t auth_key_add_const(__uint128_t a, __uint128_t c) {
    uint64_t delta_c = mult_mod((uint64_t)c, (uint64_t)delta);
    uint64_t new_mac = add_mod(MAC(a), PR - delta_c);
    return MAKE_AUTH(0, new_mac);
  }

  // verifier: subtract 2 Keys
  // return: [a] - [b]
  __uint128_t auth_key_subtract(__uint128_t a, __uint128_t b) {
    uint64_t new_mac = add_mod(MAC(a), PR - MAC(b));
    return MAKE_AUTH(0, new_mac);
  }

  // verifier: multiplies Key (mac field) with a scalar constant c.
  // BOB's val=0 stays 0; mac becomes mac · c.
  __uint128_t auth_key_mul_const(__uint128_t a, __uint128_t c) {
    uint64_t new_mac = mult_mod(MAC(a), (uint64_t)c);
    return MAKE_AUTH(0, new_mac);
  }

  uint64_t communication() { return io->send_counter; }

  /* ---------------------debug functions----------------------*/

  void check_auth_mac(__uint128_t *auth, int64_t len) {
    if (party == ALICE) {
      io->send_data(auth, len * sizeof(__uint128_t));
    } else {
      std::vector<__uint128_t> auth_recv(len);
      io->recv_data(auth_recv.data(), len * sizeof(__uint128_t));
      for (int64_t i = 0; i < len; ++i) {
        uint64_t val   = VAL(auth_recv[i]);
        uint64_t mac_a = MAC(auth_recv[i]);
        uint64_t mac_b = MAC(auth[i]);
        uint64_t recomputed = mult_mod(val, (uint64_t)delta);
        recomputed = add_mod(recomputed, mac_b);
        if (mac_a != recomputed) {
          std::cout << "authenticated mac error at: " << i << std::endl;
          abort();
        }
      }
    }
  }

  void check_compute_mul(__uint128_t *a, __uint128_t *b, __uint128_t *c,
                         int64_t len) {
    if (party == ALICE) {
      io->send_data(a, len * sizeof(__uint128_t));
      io->send_data(b, len * sizeof(__uint128_t));
      io->send_data(c, len * sizeof(__uint128_t));
    } else {
      std::vector<__uint128_t> ar(len), br(len), cr(len);
      io->recv_data(ar.data(), len * sizeof(__uint128_t));
      io->recv_data(br.data(), len * sizeof(__uint128_t));
      io->recv_data(cr.data(), len * sizeof(__uint128_t));
      for (int64_t i = 0; i < len; ++i) {
        uint64_t product = mult_mod(VAL(ar[i]), VAL(br[i]));
        expecting(product == VAL(cr[i]), "wrong product");
        uint64_t recomputed = mult_mod(product, (uint64_t)delta);
        recomputed = add_mod(recomputed, MAC(c[i]));
        expecting(recomputed == MAC(cr[i]), "wrong mac");
      }
    }
  }
};
}  // namespace emp

#endif
