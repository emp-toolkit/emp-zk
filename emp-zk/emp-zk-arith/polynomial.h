#ifndef FP_POLY_H__
#define FP_POLY_H__

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/ostriple.h"
#include "emp-zk/emp-zk-bool/emp-zk-bool.h"

namespace emp {
using namespace std;

class FpPolyProof {
public:
  static FpPolyProof *fppolyproof;
  int party;
  BoolIO *io;
  uint64_t delta;
  int64_t buffer_sz = 1024;
  std::vector<uint64_t> buffer;
  std::vector<uint64_t> buffer1;
  FpOSTriple *ostriple;
  int64_t num;

  FpPolyProof(int party, BoolIO *io, FpOSTriple *ostriple) {
    this->party = party;
    this->io = io;
    this->ostriple = ostriple;
    buffer.resize(buffer_sz);
    if (party == ALICE) {
      buffer1.resize(buffer_sz);
    } else {
      this->delta = LOW64(ostriple->delta);
    }
    num = 0;
  }

  ~FpPolyProof() { batch_check(); }

  void batch_check() {
    if (num == 0)
      return;
    uint64_t seed;
    io->flush();
    std::vector<uint64_t> chi(num);
    __uint128_t ope_data;
    uint64_t check_sum[2];
    if (party == ALICE) {
      io->recv_data(&seed, sizeof(uint64_t));

      uni_hash_coeff_gen(chi.data(), seed, num);

      check_sum[0] = vector_inn_prdt_sum_red(chi.data(), buffer.data(), num);
      check_sum[1] = vector_inn_prdt_sum_red(chi.data(), buffer1.data(), num);
      ostriple->vole->next_n((AuthValueFp *)&ope_data, 1);

      check_sum[0] = add_mod(check_sum[0], MAC(ope_data));
      check_sum[1] = add_mod(check_sum[1], VAL(ope_data));
      io->send_data(check_sum, 2 * sizeof(uint64_t));
    } else {
      PRG prg;
      prg.random_data_unaligned(&seed, sizeof(uint64_t));
      seed = mod(seed);
      io->send_data(&seed, sizeof(uint64_t));

      uni_hash_coeff_gen(chi.data(), seed, num);
      uint64_t B = vector_inn_prdt_sum_red(chi.data(), buffer.data(), num);
      ostriple->vole->next_n((AuthValueFp *)&ope_data, 1);
      B = add_mod(B, MAC(ope_data));
      io->recv_data(check_sum, 2 * sizeof(uint64_t));

      uint64_t tmp = mult_mod(check_sum[1], delta);
      tmp = add_mod(B, tmp);
      expecting(tmp == check_sum[0], "polynomial zkp fails");
    }
    num = 0;
  }

  inline void zkp_poly_deg2(const __uint128_t *polyx, const __uint128_t *polyy,
                            const uint64_t *coeff, int64_t len) {
    if (num >= buffer_sz)
      batch_check();

    if (party == ALICE) {
      uint64_t A0 = 0, A1 = 0;
      uint64_t w0, w1, m0, m1, tmp;
      for (int64_t i = 0; i < len; ++i) {
        w0 = VAL(polyx[i]);
        m0 = MAC(polyx[i]);
        w1 = VAL(polyy[i]);
        m1 = MAC(polyy[i]);

        tmp = mult_mod(m0, m1);
        tmp = mult_mod(coeff[i + 1], tmp);
        A0 = add_mod(A0, tmp);

        tmp = add_mod(mult_mod(m0, w1), mult_mod(m1, w0));
        tmp = mult_mod(coeff[i + 1], tmp);
        A1 = add_mod(A1, tmp);
      }
      buffer[num] = A0;
      buffer1[num] = A1;
    } else {
      uint64_t B = 0;
      uint64_t tmp;
      for (int64_t i = 0; i < len; ++i) {
        tmp = mult_mod(MAC(polyx[i]), MAC(polyy[i]));
        tmp = mult_mod(coeff[i + 1], tmp);
        B = add_mod(B, tmp);
      }
      tmp = mult_mod(delta, delta);
      tmp = mult_mod(coeff[0], tmp);
      B = add_mod(B, tmp);
      buffer[num] = B;
    }
    num++;
  }

  inline void zkp_inner_prdt(const __uint128_t *polyx, const __uint128_t *polyy,
                             uint64_t constant, int64_t len) {
    if (num >= buffer_sz)
      batch_check();

    if (party == ALICE) {
      uint64_t A0 = 0, A1 = 0;
      uint64_t w0, w1, m0, m1, tmp;
      for (int64_t i = 0; i < len; ++i) {
        w0 = VAL(polyx[i]);
        m0 = MAC(polyx[i]);
        w1 = VAL(polyy[i]);
        m1 = MAC(polyy[i]);

        tmp = mult_mod(m0, m1);
        A0 = add_mod(A0, tmp);

        tmp = add_mod(mult_mod(m0, w1), mult_mod(m1, w0));
        A1 = add_mod(A1, tmp);
      }
      buffer[num] = A0;
      buffer1[num] = A1;
    } else {
      uint64_t B = 0;
      uint64_t tmp;
      for (int64_t i = 0; i < len; ++i) {
        tmp = mult_mod(MAC(polyx[i]), MAC(polyy[i]));
        B = add_mod(B, tmp);
      }
      tmp = mult_mod(delta, delta);
      tmp = mult_mod(constant, tmp);
      B = add_mod(B, tmp);
      buffer[num] = B;
    }
    num++;
  }
};
inline FpPolyProof *FpPolyProof::fppolyproof = nullptr;
}  // namespace emp

#endif
