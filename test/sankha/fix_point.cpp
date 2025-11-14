#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/extensions/floats.h"
#include <iostream>
using namespace emp;
using namespace std;

#define LOW64(x) _mm_extract_epi64((block)x, 0)
#define HIGH64(x) _mm_extract_epi64((block)x, 1)

int port, party;
const int threads = 1;
int test_n = 1;

void setup_stuff(BoolIO<NetIO> *ios[threads], int party){
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;
  ios[0]->counter = 0;
}


void seq_mult_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "------ FIXED POINT MULTIPLICATION ------\n";
  setup_stuff(ios, party);

  srand(time(NULL));
  int sz = test_n;
  // cleartext
  uint64_t *d = new uint64_t[sz];
  uint64_t *e = new uint64_t[sz];
  uint64_t *f = new uint64_t[sz];

  // auth
  IntFp *di = new IntFp[sz];
  IntFp *ei = new IntFp[sz];
  IntFp *fi = new IntFp[sz];

  ios[0]->counter = 0;
  // cleartext multiplication and sending answers
  if(party == ALICE){
    // prover

    for(int i = 0; i < sz; i++){
      d[i] = rand() % PR;
      e[i] = rand() % PR;
      f[i] = mult_mod(d[i], e[i]);

      f[i] = PR - f[i];
      ios[0]->send_data(&f[i], sizeof(uint64_t));
    }
  } else {
    // verifier

    for(int i = 0; i < sz; i++){
      ios[0]->recv_data(&f[i], sizeof(uint64_t));
    }
  }

  ios[0]->counter = 0;
  auto start = clock_start();
  // witness authentication
  for(int i = 0; i < sz; i++){
    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);
    fi[i] = di[i] * ei[i];  
  }

  // actual "proving" business
  for (int i = 0; i < sz; i++) {
    fp_zkp_inner_prdt<BoolIO<NetIO>>(&di[i], &ei[i], f[i], 1);
  }
  finalize_zk_arith<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter/sz << " bytes \n";

}


void seq_add_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "------ FIXED POINT ADDITION ------\n";
  setup_stuff(ios, party);

  srand(time(NULL));
  int sz = test_n;
  // cleartext
  uint64_t *d = new uint64_t[sz];
  uint64_t *e = new uint64_t[sz];
  uint64_t *f = new uint64_t[sz];

  // auth
  IntFp *di = new IntFp[sz];
  IntFp *ei = new IntFp[sz];
  IntFp *fi = new IntFp[sz];

  ios[0]->counter = 0;
  // cleartext multiplication and sending answers
  if(party == ALICE){
    // prover

    for(int i = 0; i < sz; i++){
      d[i] = rand() % PR;
      e[i] = rand() % PR;
      f[i] = add_mod(d[i], e[i]);

      ios[0]->send_data(&f[i], sizeof(uint64_t));
    }
  } else {
    // verifier

    for(int i = 0; i < sz; i++){
      ios[0]->recv_data(&f[i], sizeof(uint64_t));
    }
  }

  auto start = clock_start();
  // witness authentication
  for(int i = 0; i < sz; i++){
    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);
    fi[i] = di[i] + ei[i];
  }

  // actual "proving" business
  batch_reveal_check(fi, f, sz);
  finalize_zk_arith<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void seq_comp_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIXED POINT COMPARISON  ------\n";
  
  auto start = clock_start();
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;

  sync_zk_bool<BoolIO<NetIO>>();

  uint64_t* a = new uint64_t[sz];
  uint64_t* b = new uint64_t[sz];

  IntFp* a_field = new IntFp[sz];
  IntFp* b_field = new IntFp[sz];

  Integer* a_int = new Integer[sz];
  Integer* b_int = new Integer[sz];

  Bit* c = new Bit[sz];

  start = clock_start();

  for(int i = 0; i < sz; i++){
    a[i] = rand() % PR;   // max value = 2^61 - 2; represented in 64 bits
    a_field[i] = IntFp(a[i], ALICE);
  
    b[i] = rand() % PR;
    b_field[i] = IntFp(b[i], ALICE);
  }

  ios[0]->counter = 0;

  arith2bool<BoolIO<NetIO>>(a_int, a_field, sz);
  arith2bool<BoolIO<NetIO>>(b_int, b_field, sz);

  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
  
  for(int i = 0; i < sz; i++){
    c[i] = b_int[i].geq(a_int[i]);
  }

  bool2arith<BoolIO<NetIO>>(a_field, a_int, sz);
  bool2arith<BoolIO<NetIO>>(b_field, b_int, sz);

  // batch_reveal_check(a_field, a, sz);

  finalize_zk_arith<BoolIO<NetIO>>();
  finalize_zk_bool<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);

  cout << "\nAvg. time for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void seq_relu_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIXED POINT RELU  ------\n";
  
  auto start = clock_start();
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;

  sync_zk_bool<BoolIO<NetIO>>();

  uint64_t* a = new uint64_t[sz];
  IntFp* a_field = new IntFp[sz];
  Integer* a_int = new Integer[sz];

  Bit* c = new Bit[sz];
  Integer* c_int = new Integer[sz];
  IntFp* c_field = new IntFp[sz];


  start = clock_start();

  for(int i = 0; i < sz; i++){
    a[i] = rand() % PR;   // max value = 2^61 - 2; represented in 64 bits
    a_field[i] = IntFp(a[i], ALICE);
  }

  ios[0]->counter = 0;

  arith2bool<BoolIO<NetIO>>(a_int, a_field, sz);

  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
  
  for(int i = 0; i < sz; i++){
    c[i] = a_int[i].geq(Integer(62, 0, PUBLIC));
    c_int[i] = Integer(61, &c[i], ALICE);
  }

  bool2arith<BoolIO<NetIO>>(c_field, c_int, sz);
  for(int i = 0; i < sz; i++){
    a_field[i] = c_field[i]*a_field[i];  
  }

  // batch_reveal_check(a_field, a, sz);

  finalize_zk_arith<BoolIO<NetIO>>();
  finalize_zk_bool<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);

  cout << "\nAvg. time for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void fxp_comp(BoolIO<NetIO> *ios[threads], int party) {
  setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

  Integer* a = new Integer[2];
  IntFp* a_Fp = new IntFp[2];

  a[0] = Integer(62, -4.537, PUBLIC);
  a[1] = Integer(62, 2.59, PUBLIC);

  cout << a[0].reveal<string>() << "\t" << (a[0] > Integer(62, 0, PUBLIC)).reveal<string>() << "\n";
  cout << a[1].reveal<string>() << (a[1] > Integer(62, 0, PUBLIC)).reveal<string>() << "\n";

  bool2arith<BoolIO<NetIO>>(a_Fp, a, 2);

  IntFp c = a_Fp[0]*a_Fp[1];
  Integer d(62, 0, PUBLIC);

  arith2bool<BoolIO<NetIO>>(&d, &c, 1);
  
  cout << d.reveal<string>() << " \n";
}


int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);
  
  if(argc > 3)  test_n = atoi(argv[3]);
    
  std::cout << std::endl
            << "------------ circuit zero-knowledge proof test ------------"
            << std::endl
            << std::endl;

  // seq_mult_fxp(ios, party);
  // seq_add_fxp(ios, party);
  // seq_comp_fxp(ios, party);
  fxp_comp(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
