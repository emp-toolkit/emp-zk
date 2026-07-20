#ifndef EDABITS_H__
#define EDABITS_H__

#include <algorithm>
#include <bitset>

#include "emp-ot/emp-ot.h"
#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/edabit/doub_auth_helper.h"
#include "emp-zk/emp-zk-bool/emp-zk-bool.h"

namespace emp {
using namespace std;

// AuthValue accessors (val-first: val in low 64, mac in high 64).
#define VAL(x) _mm_extract_epi64((block)x, 0)
#define MAC(x) _mm_extract_epi64((block)x, 1)
#define MAKE_AUTH(val_, mac_) \
    ((__uint128_t)makeBlock((uint64_t)(mac_), (uint64_t)(val_)))

class EdaBits {
public:
  static EdaBits *conv;
  int party;
  BoolIO *io;

  block delta_f2;
  std::vector<ZKInt> bool_candidate;
  __uint128_t delta_fp;
  std::vector<__uint128_t> arith_candidate;

  FpVOLE<AuthValueFp> *cot_fp = nullptr;

  DoubAuthHelper *auth_helper = nullptr;

  uint32_t np_pt, np_rg, np_sz;
  uint32_t rand_pt;

  uint32_t edabit_num, edabit_offset;

  const uint32_t N = 800000, B = 2, C = 2; // B=2, C=2, N>=741455
                                           // B=3, C=3, N>=6251
  uint32_t ell;
  uint32_t Bm1, ell_faulty;

  ZKInt int_boo_pr, int_boo_zero, int_boo_pr_plus_two;
  ZKBoolSession &zk;   // bool session: runtime input + the bool engine

  EdaBits(ZKBoolSession &zk, BoolIO *io, FpVOLE<AuthValueFp> *cot_fp) : zk(zk) {
    this->party = zk.party();
    this->io = io;
    this->cot_fp = cot_fp;
    if (party == BOB) {
      this->delta_fp = cot_fp->delta();
    }

    this->np_sz = cot_fp->chunk_aligned_buf_sz();
    this->np_pt = 0;
    this->np_rg = 0;
    this->edabit_offset = 0;
    this->rand_pt = 0;
    this->edabit_num = 0;
    arith_candidate.resize(cot_fp->chunk_aligned_buf_sz());
    cot_fp->next_n((AuthValueFp *)arith_candidate.data(), cot_fp->chunk_aligned_buf_sz());

    this->ell = B * N + C; // batch size
    this->ell_faulty = ell - N;
    this->Bm1 = B - 1;
    bool_candidate.resize(ell);

    auth_helper = new DoubAuthHelper(zk, io);

    int_boo_pr = zk.input<ZKInt>(PUBLIC, PR, 62);
    int_boo_zero = zk.input<ZKInt>(PUBLIC, 0, 62);
    // Negative-value correction added in the f2 overflow check below
    // (the `+ int_boo_pr_plus_two` select); the +2 vs PR is specific to that path.
    int_boo_pr_plus_two = zk.input<ZKInt>(PUBLIC, PR + 2, 62);
  }

  ~EdaBits() {
    if (!auth_helper->triple_equality_check())
      error("cut and choose fails");
    if (auth_helper != nullptr)
      delete auth_helper;
  }

  void install_boolean(block delta_f2) {
    this->delta_f2 = delta_f2;
    auth_helper->set_delta(delta_f2, delta_fp);
  }

  void edabits_gen_backend() {
    // auto start = clock_start();
    //  If the buffer is used up, refill the Fp shares
    if (np_pt + ell > np_sz) {
      cot_fp->next_n((AuthValueFp *)arith_candidate.data(), cot_fp->chunk_aligned_buf_sz());
      np_pt = 0;
    }
    np_rg = np_pt + ell;

    // Input \ell Fp shares into boolean circuits
    if (party == ALICE) {
      for (uint32_t i = 0; i < ell; ++i)
        bool_candidate[i] = zk.input<ZKInt>(ALICE, VAL(arith_candidate[np_pt + i]), 
            62);
    } else {
      for (uint32_t i = 0; i < ell; ++i)
        bool_candidate[i] = zk.input<ZKInt>(ALICE, 0, 62);
    }
    zk.flush();

    // Generate a random point to do the permutation
    rand_pt = random_point(ell_faulty);

    // Open S_o TODO overflow
    if (party == ALICE)
      auth_helper->open_check_send(bool_candidate.data() + N + rand_pt,
                                   arith_candidate.data() + np_pt + N + rand_pt, C);
    else
      auth_helper->open_check_recv(bool_candidate.data() + N + rand_pt,
                                   arith_candidate.data() + np_pt + N + rand_pt, C);

    // bucketing
    uint32_t buc_start = fp_index(np_pt + N + rand_pt + C);
    uint32_t buc_start1 = f2_index(rand_pt + N + C);
    std::vector<__uint128_t> fp_to_check(N);
    std::vector<ZKInt> f2_to_check(N);
    for (uint32_t j = 0; j < Bm1; ++j) { // TODO parameter
      uint32_t ifp0 = np_pt;
      uint32_t ifp1 = fp_index(buc_start + j);
      uint32_t if21 = f2_index(buc_start1 + j);
      for (uint32_t i = 0; i < N; ++i) {
        fp_to_check[i] =
            intfp_add(arith_candidate[ifp0++], arith_candidate[ifp1]);
        ifp1 = fp_index(ifp1 + Bm1);
        f2_to_check[i] = bool_candidate[i] + bool_candidate[if21];
        f2_to_check[i] = f2_to_check[i].select(
            f2_to_check[i][61], f2_to_check[i] + int_boo_pr_plus_two);
        if21 = f2_index(if21 + Bm1);
        // TODO boolean addition and selection costs a lot, and it should be
        // subtraction
      }
      zk.flush();
      if (party == ALICE)
        auth_helper->open_check_send(f2_to_check.data(), fp_to_check.data(), N);
      else
        auth_helper->open_check_recv(f2_to_check.data(), fp_to_check.data(), N);
    }
    if (!auth_helper->triple_equality_check())
      error("cut and choose fails");
    edabit_num = N;
    edabit_offset = 0;
    // std::cout << "edabits generation: " << time_from(start)/N << "
    // us/edabits" << std::endl;
  }

  __uint128_t bool2arith(ZKInt in) {
    uint32_t edab_f2, edab_fp;
    uint64_t diff;
    ZKInt diff_bool;

    next_edabits(edab_f2, edab_fp);
    diff_bool = in - bool_candidate[edab_f2];
    diff_bool = diff_bool.select(diff_bool[61], diff_bool + int_boo_pr);

    if (party == ALICE)
      auth_helper->open_check_send(&diff, &diff_bool, 1);
    else
      auth_helper->open_check_recv(&diff, &diff_bool, 1);
    return intfp_add_const(arith_candidate[edab_fp], diff);
  }

  void bool2arith(__uint128_t *out, const ZKInt *in, int64_t len) {
    int64_t off = 0;
    while (off < len) {
      // Cap each chunk at the edabits remaining in the current buffer
      // (or N, the size of a fresh batch) so the chunk never spans a
      // regen boundary — edabits_gen_backend() inside next_edabits
      // does its own I/O and can't be interleaved with a bulk send.
      int64_t avail = (edabit_num > 0) ? (int64_t)edabit_num : (int64_t)N;
      int64_t num = std::min<int64_t>(len - off, avail);

      uint32_t edab_f2;
      std::vector<uint32_t> edab_fp(num);
      std::vector<uint64_t> diff(num);
      std::vector<ZKInt> diff_bool(num);
      for (int64_t i = 0; i < num; ++i) {
        next_edabits(edab_f2, edab_fp[i]);
        diff_bool[i] = in[off + i] - bool_candidate[edab_f2];
        diff_bool[i] = diff_bool[i].select(diff_bool[i][61],
                                           diff_bool[i] + int_boo_pr);
      }
      if (party == ALICE)
        auth_helper->open_check_send(diff.data(), diff_bool.data(), num);
      else
        auth_helper->open_check_recv(diff.data(), diff_bool.data(), num);
      io->flush();
      for (int64_t i = 0; i < num; ++i)
        out[off + i] = intfp_add_const(arith_candidate[edab_fp[i]], diff[i]);
      off += num;
    }
  }

  ZKInt arith2bool(__uint128_t in) {
    uint32_t edab_fp, edab_f2;
    __uint128_t sum_fp;
    uint64_t sum;
    next_edabits(edab_f2, edab_fp);

    sum_fp = intfp_add(arith_candidate[edab_fp], in);
    if (party == ALICE)
      auth_helper->open_check_send(&sum, &sum_fp, 1);
    else
      auth_helper->open_check_recv(&sum, &sum_fp, 1);

    ZKInt sum_boo = zk.input<ZKInt>(PUBLIC, sum, 62);
    sum_boo = sum_boo - bool_candidate[edab_f2];
    return sum_boo.select(sum_boo[61], sum_boo + int_boo_pr);
  }

  void arith2bool(ZKInt *out, const __uint128_t *in, int64_t len) {
    int64_t off = 0;
    while (off < len) {
      int64_t avail = (edabit_num > 0) ? (int64_t)edabit_num : (int64_t)N;
      int64_t num = std::min<int64_t>(len - off, avail);

      uint32_t edab_fp;
      std::vector<uint32_t> edab_f2(num);
      std::vector<__uint128_t> sum_fp(num);
      std::vector<uint64_t> sum(num);
      for (int64_t i = 0; i < num; ++i) {
        next_edabits(edab_f2[i], edab_fp);
        sum_fp[i] = intfp_add(arith_candidate[edab_fp], in[off + i]);
      }
      if (party == ALICE)
        auth_helper->open_check_send(sum.data(), sum_fp.data(), num);
      else
        auth_helper->open_check_recv(sum.data(), sum_fp.data(), num);
      io->flush();
      for (int64_t i = 0; i < num; ++i) {
        ZKInt sum_boo = zk.input<ZKInt>(PUBLIC, sum[i], 62);
        sum_boo = sum_boo - bool_candidate[edab_f2[i]];
        out[off + i] =
            sum_boo.select(sum_boo[61], sum_boo + int_boo_pr);
      }
      off += num;
    }
  }

  uint32_t random_point(uint32_t range) {
    uint32_t rand_pt = 0;
    if (party == ALICE) {
      io->recv_data(&rand_pt, sizeof(uint32_t));
    } else {
      PRG prg;
      prg.random_data_unaligned(&rand_pt, sizeof(uint32_t));
      rand_pt = rand_pt % range;
      io->send_data(&rand_pt, sizeof(uint32_t));
      io->flush();
    }
    return rand_pt;
  }

  uint32_t fp_index(uint32_t offset) {
    if (offset >= np_rg)
      offset -= (ell_faulty);
    return offset;
  }

  uint32_t f2_index(uint32_t offset) {
    if (offset >= ell)
      offset -= (ell_faulty);
    return offset;
  }

  void next_edabits(uint32_t &f2_indexin, uint32_t &fp_index) {
    if (edabit_num == 0) {
      np_pt = np_rg;
      edabits_gen_backend();
    }
    f2_indexin = edabit_offset;
    fp_index = np_pt + edabit_offset;
    edabit_offset++;
    edabit_num--;
  }

  __uint128_t intfp_add_const(__uint128_t a, uint64_t b) {
    if (party == ALICE) {
      // ALICE has (val, mac). Adding constant b to val only.
      uint64_t val = add_mod(VAL(a), b);
      uint64_t mac = MAC(a);
      return MAKE_AUTH(val, mac);
    } else {
      // BOB (Δ-holder): mac -= b · Δ; val stays 0.
      uint64_t mb = mult_mod(b, (uint64_t)delta_fp);
      mb = PR - mb;
      uint64_t mac = add_mod(MAC(a), mb);
      return MAKE_AUTH(0, mac);
    }
  }

  __uint128_t intfp_add(__uint128_t a, __uint128_t b) {
    if (party == ALICE) {
      // Both halves add element-wise.
      uint64_t val = add_mod(VAL(a), VAL(b));
      uint64_t mac = add_mod(MAC(a), MAC(b));
      return MAKE_AUTH(val, mac);
    } else {
      // BOB: only macs combine; vals stay 0.
      uint64_t mac = add_mod(MAC(a), MAC(b));
      return MAKE_AUTH(0, mac);
    }
  }

  // DEBUG
  void sender_check_edabits(uint32_t edab_f2, uint32_t edab_fp) {
    if (party == ALICE) {
      uint64_t a = sender_check_int_value(bool_candidate[edab_f2]);
      uint64_t b = sender_check_int_value(arith_candidate[edab_fp]);
      if (a != b)
        error("edabit error!");
    }
  }

  bool sender_check_conversion(ZKInt in2, __uint128_t inp) {
    if (party == ALICE) {
      uint64_t a = sender_check_int_value(in2);
      expecting(a < PR, "edabit conversion check: boolean value out of field range");
      uint64_t b = sender_check_int_value(inp);
      expecting(b < PR, "edabit conversion check: arithmetic value out of field range");
      if (a != b) {
        return false;
      }
    }
    return true;
  }

  uint64_t sender_check_int_value(ZKInt in) {
    std::bitset<64> val = 0;
    int bit_len = in.width();
    for (int i = 0; i < bit_len; ++i)
      val.set(i, getLSB(in[i].w.label));
    return val.to_ullong();
  }

  uint64_t sender_check_int_value(__uint128_t in) {
    return VAL(in);
  }
};
inline EdaBits *EdaBits::conv = nullptr;
}  // namespace emp

#endif
