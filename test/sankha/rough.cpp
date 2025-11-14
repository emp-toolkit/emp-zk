#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/extensions/floats.h"
#include "emp-zk/extensions/lowmc.h"
#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int test_n = 1;


void test_field_comp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIELD COMPARISON  ------\n";
  
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

  for(int i = 0; i < sz; i++){
    a[i] = rand() % PR;   // max value = 2^61 - 2; represented in 64 bits
    a_field[i] = IntFp(a[i], ALICE);
  
    b[i] = (a[i] + 1) % PR;
    b_field[i] = IntFp(b[i], ALICE);
  }

  start = clock_start();
  ios[0]->counter = 0;

  arith2bool<BoolIO<NetIO>>(a_int, a_field, sz);
  arith2bool<BoolIO<NetIO>>(b_int, b_field, sz);

  for(int i = 0; i < sz; i++){
    c[i] = b_int[i].geq(a_int[i]);
    // cout << c[i].reveal<string>(PUBLIC) << " ";
  }

  // batch_reveal_check(a_field, a, sz);

  auto timecrypto = time_from(start);
  cout << "\nAvg. time for ReLU " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_arith<BoolIO<NetIO>>();
  finalize_zk_bool<BoolIO<NetIO>>();

}

void test_float_comp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOATING POINT COMPARISON ------\n";
  
  auto start = clock_start();
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


  srand(time(NULL));
  int sz = test_n;
  // cleartext
  float *d = new float[sz];
  float *e = new float[sz];
  int *f = new int[sz];

  // auth
  Float *di = new Float[sz];
  Float *ei = new Float[sz];
  Integer *dint = new Integer[sz];
  Integer *eint = new Integer[sz];

  Bit *fi = new Bit[sz];

  for (int i = 0; i < sz; i++){
    d[i] = rand();
    e[i] = rand();

    di[i] = Float(d[i], ALICE);
    ei[i] = Float(e[i], ALICE);
    fi[i] = Bit(0, ALICE);

    f[i] = (d[i] >= e[i]);
  }

  ios[0]->counter = 0;
  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    dint[i] = FloatToInt62(di[i], 16);
    eint[i] = FloatToInt62(ei[i], 16);

    // fi[i] = di[i].less_equal(ei[i]);
    fi[i] = dint[i].geq(eint[i]);
    if(fi[i].reveal<bool>() != (bool) f[i]){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_plain_prot();

  finalize_zk_arith<BoolIO<NetIO>>();
}

void test_integer(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ INTEGER COMPARISON ------\n";
  
  auto start = clock_start();
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  // setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;
  ios[0]->counter = 0;

  start = clock_start();
  int sz = test_n;
  for(int i = 0; i < sz; i++){
    Integer a(62, 5, ALICE);
    Integer b(62, 20, ALICE);
    Bit ret = a.geq(b);
  }

  finalize_zk_bool<BoolIO<NetIO>>();
  auto timecrypto = time_from(start);

  std::cout << "Time for " << sz << " instances : " << timecrypto / sz << " us" << std::endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void test_lowmc(BoolIO<NetIO> *ios[threads], int party) {
  unsigned nblocks = 10;
  unsigned test_sz = nblocks * blocksize;
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  sync_zk_bool<BoolIO<NetIO>>();

  bool *key_b = new bool[keysize];
  bool *ptx_b = new bool[test_sz];
  bool *ctx_b = new bool[test_sz];
  bool *ctx_rev = new bool[test_sz];
  Bit *ptx = new Bit[test_sz];
  Bit *ctx = new Bit[test_sz];

  ios[0]->counter = 0;

  PRG prg;
  prg.reseed(&all_one_block);
  prg.random_bool(key_b, keysize);
  // cout<<"key:";for(int i = 0; i < keysize; ++i)cout<<key_b[i];cout<<endl;
  prg.random_bool(ptx_b, test_sz);
  // cout<<"ptx:";for(int i = 0; i < test_sz; ++i)cout<<ptx_b[i];cout<<endl;
  ProtocolExecution::prot_exec->feed((block *)ptx, ALICE, ptx_b, test_sz);
  cout << "Comm: " << ios[0]->counter << "\n";

  ZKLowMC *lowmc = new ZKLowMC(key_b);
  cout << "Comm: " << ios[0]->counter << "\n";

  lowmc->encrypt(ctx_b, ptx_b, nblocks);
  // cout<<"ctx loc:";for(int i = 0; i < test_sz; ++i)cout<<ctx_b[i];cout<<endl;
  cout << "Comm: " << ios[0]->counter << "\n";

  auto start = clock_start();
  lowmc->encrypt(ctx, ptx, nblocks);
  double tt = time_from(start);
  cout << "Comm: " << ios[0]->counter << "\n";

  ProtocolExecution::prot_exec->reveal(ctx_rev, PUBLIC, (block *)ctx, test_sz);
  cout << "Comm: " << ios[0]->counter << "\n";

  bool cheated = finalize_zk_bool<BoolIO<NetIO>>();
  if (cheated)
    error("cheated\n");
  cout << "Comm: " << ios[0]->counter << "\n";

  // cout<<"ctx rev:";for(int i = 0; i < test_sz;
  // ++i)cout<<ctx_rev[i];cout<<endl;
  std::cout << "check encryption consistency" << std::endl;
  std::cout << memcmp(ctx_b, ctx_rev, test_sz * sizeof(bool)) << std::endl;

  std::cout << "time: " << tt / nblocks << " us" << std::endl;

  delete[] key_b;
  delete[] ptx_b;
  delete[] ctx_b;
  delete[] ctx_rev;
  delete[] ptx;
  delete[] ctx;
}


void fix_relu(BoolIO<NetIO> *ios[threads], int party){
  setup_plain_prot(false, "");
  
  // float x = -0.532;
  // int64_t x_tilde = x * (1 << 16);

  uint64_t x_tilde = (PR - 3) % PR;
  Integer x_int(62, x_tilde, PUBLIC);

  Integer zero(62, 0, PUBLIC);

  cout << x_int.reveal<string>() << "\n";
  cout << x_int.geq(zero).reveal<string>() << "\n";
}


void fix_relu_IntField(BoolIO<NetIO> *ios[threads], int party){
  // setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  
  float x = -0.532;
  int64_t x_tilde = x * (1 << 16);
  Integer x_int(62, x_tilde, PUBLIC);
  Integer y_int(62, 0, PUBLIC);
  IntFp x_Fp(0, PUBLIC);
  IntFp y_Fp(2, PUBLIC);

  cout << x_int.reveal<string>() << "\n";
  bool2arith<BoolIO<NetIO>>(&x_Fp, &x_int, 1);
  cout << x_int.reveal<string>() << "\n";

  x_Fp = x_Fp;

  arith2bool<BoolIO<NetIO>>(&x_int, &x_Fp, 1);
  cout << x_int.reveal<string>() << "\n";

  Integer zero(62, 0, PUBLIC);

  cout << y_int.reveal<string>() << "\n";
  cout << y_int.geq(zero).reveal<string>() << "\n";
}

// void fix_inner_product(BoolIO<NetIO> *ios[threads], int party){
//   setup_plain_prot(false, "");
//   float x = -0.532; 
//   float y = 0.392;

//   int64_t x_tilde = x * (1 << 16);
//   int64_t y_tilde = y * (1 << 16);

//   Integer x_int(62, x_tilde, PUBLIC);
//   Integer y_int(62, y_tilde, PUBLIC);
//   Integer z_int(62, 0, PUBLIC);

//   z_int = x_int * y_int;
  
//   cout << z_int.reveal<string>() << "\n";
//   cout << z_int.reveal<uint64_t>() << "\n";
// }

void fix_inner_product(BoolIO<NetIO> *ios[threads], int party){
  setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  
  int64_t a = (-1.459) * (1 << 16);
  IntFp a_Fp(a, PUBLIC);

  Integer a_int(62, 0, PUBLIC);
  arith2bool<BoolIO<NetIO>>(&a_int, &a_Fp, 1);

  Integer zero(62, 0, PUBLIC);

  cout << a_int.reveal<string>() << "\n";
  cout << a_int.geq(zero).reveal<string>() << "\n";

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

  // seq_float2fix(ios, party);
  // seq_fix2float(ios, party);
  // test_field_comp(ios, party);
  // test_float_comp(ios, party);
  // test_integer(ios, party);
  // test_lowmc(ios, party);

  // fix_relu(ios, party);
  // fix_relu_IntField(ios, party);
  fix_inner_product(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
