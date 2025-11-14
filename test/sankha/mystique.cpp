#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/extensions/floats.h"
#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int test_n = 1;


void seq_float2fix(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOAT TO FIX CONVERSION ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;

  srand(time(NULL));
  start = clock_start();
  for(int i = 0; i < sz; i++){
    Float a((float) 3.149, ALICE);

    Integer a_int(62, 0, ALICE);
    a_int = FloatToInt62(a, 16);

    // cout << "a_fx = " << a_int.reveal<int>() << "\n";
  }

  finalize_plain_prot();
  finalize_zk_arith<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);
  cout << "Avg. time for float to fix conversion for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void seq_fix2float(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIX TO FLOAT CONVERSION ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;

  srand(time(NULL));
  start = clock_start();
  for(int i = 0; i < sz; i++){
    Integer a_int(62, 206372, ALICE);

    Float a_float(0, ALICE);
    a_float = Int62ToFloat(a_int, 16);

    // cout << "a_fx = " << a_int.reveal<int>() << "\n";
  }

  finalize_plain_prot();
  finalize_zk_arith<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);
  cout << "Avg. time for float to fix conversion for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}


void seq_relu_float(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOATING POINT RELU  ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;
  float* a_fl = new float[sz];
  Float* a_float = new Float[sz];
  Float* a_relu = new Float[sz];

  ios[0]->counter = 0;
  srand(time(NULL));
  start = clock_start();
  for(int i = 0; i < sz; i++){
    // int64_t a_int = (rand() % PR);
    // cout << "a_int (clt) = " << a_int << "\n";
    a_fl[i] = 3.141;
    a_fl[i] = (1 - 2*(i & 1))*a_fl[i];

    a_float[i] = Float(a_fl[i], ALICE);
    a_relu[i] = Float(0, ALICE);

    a_relu[i] = a_float[i].If(a_float[i].less_than(0), 0);

    // cout << "a_fl (clt) = " << a_fl << "\n";
    // cout << "a_float (zk) = " << a_float.reveal<double>() << "\n";
    // cout << "a_relu = " << a_relu.reveal<double>() << "\n\n";
  }

  auto timecrypto = time_from(start);
  cout << "Avg. time for ReLU " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_plain_prot();
  finalize_zk_arith<BoolIO<NetIO>>();

}

void seq_relu_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIXED POINT RELU  ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


  int sz = test_n;
  uint64_t* a = new uint64_t[sz];
  IntFp* ai = new IntFp[sz];
  Integer* aint = new Integer[sz];
  uint64_t* a_relu = new uint64_t[sz];

  for(int i = 0; i < sz; i++){
    a[i] = rand() % PR;
  }
  batch_feed(ai, a, sz);
  
  sync_zk_bool<BoolIO<NetIO>>();
  for(int i = 0; i < sz; i++){
    if(i & 1){
      aint[i] = Integer(62, -a[i], ALICE);
      ai[i] = ai[i].negate();
    } else {
      aint[i] = Integer(62, a[i], ALICE);
    }
  }
  sync_zk_bool<BoolIO<NetIO>>();

  ios[0]->counter = 0;
  start = clock_start();
  for(int i = 0; i < sz; i++){
    aint[i] = aint[i].select(aint[i].bits[61], Integer(62, 0, ALICE));
  }
  // bool2arith<BoolIO<NetIO>>(ai, aint, sz);

  batch_reveal(ai, a_relu, sz);  
  for(int i = 0; i < sz; i++){
    if(a[i] > (PR/2)){
      cout << "a (neg) = " << a[i] << ", a_relu = " << a_relu[i] << "\n";
      assert(a_relu[i] == 0);
    } else {
      cout << "a (pos) = " << a[i] << ", a_relu = " << a_relu[i] << "\n";
      assert(a_relu[i] == a[i]);
    }
  }
  cout << "\n";

  auto timecrypto = time_from(start);
  cout << "Avg. time for ReLU " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_arith<BoolIO<NetIO>>();
  finalize_zk_bool<BoolIO<NetIO>>();
}

void relu_test(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ RELU  ------\n";
  
  auto start = clock_start();
  setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


  ios[0]->counter = 0;
  int sz = test_n;

  uint64_t* x = new uint64_t[sz];
  IntFp* x_Fp = new IntFp[sz];
  Integer* x_f = new Integer[sz];
  Float* x_float = new Float[sz];


  for(int i = 0; i < sz; i++){
    x[i] = rand() % PR;
    cout << (int) x[i] << " ";
    x_Fp[i] = IntFp(x[i], ALICE);
    x_f[i] = Integer(62, 0, ALICE);
  }

  start = clock_start();
  arith2bool<BoolIO<NetIO>>(x_f, x_Fp, sz);
  cout << "Comm after A2B: " << ios[0]->counter / sz << " bytes \n";

  for(int i = 0; i < sz; i++){
    x_float[i] = Int62ToFloat(x_f[i], 16);
    
    // relu
    x_float[i] = x_float[i].If(x_float[i].less_than(Float(0, PUBLIC)), Float(0, ALICE));

    x_f[i] = FloatToInt62(x_float[i], 16);
  }
  cout << "Comm after ReLU: " << ios[0]->counter / sz << " bytes \n";

  bool2arith<BoolIO<NetIO>>(x_Fp, x_f, sz);
  cout << "Comm after B2A: " << ios[0]->counter / sz << " bytes \n";


  for(int i = 0; i < sz; i++){
    cout << x_Fp[i].reveal() << " ";
  }
  cout << "\n";

  finalize_zk_bool<BoolIO<NetIO>>();
  finalize_zk_arith<BoolIO<NetIO>>();


  auto timecrypto = time_from(start);
  cout << "Avg. time for float to fix conversion for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_plain_prot();
}

void a2b_opt_test(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ RELU  ------\n";
  
  auto start = clock_start();
  setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;
  uint64_t* r = new uint64_t[sz];
  IntFp* r_Fp = new IntFp[sz];
  Integer* r_f = new Integer[sz]; 

  for(int i = 0; i < sz; i++){
    r[i] = rand() % PR;
    r_Fp[i] = IntFp(r[i], ALICE);
    r_f[i] = Integer(62, 0, ALICE);
  }

  arith2bool<BoolIO<NetIO>>(r_f, r_Fp, sz);

  uint64_t* a = new uint64_t[sz];
  IntFp* a_Fp = new IntFp[sz];
  Integer* a_f = new Integer[sz];
  if(party == ALICE){
    for(int i = 0; i < sz; i++){
      a[i] = rand() % PR;
      uint64_t d = (a[i] - r[i]) % PR;
      ios[0]->send_data(&d, sizeof(uint64_t));
    }
  } else {
    ;
  }
}


void float_comp(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ FLOAT COMP  ------\n";
  
  auto start = clock_start();
  setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


  ios[0]->counter = 0;
  int sz = test_n;

  uint64_t* x = new uint64_t[sz];
  IntFp* x_Fp = new IntFp[sz];
  Integer* x_f = new Integer[sz];
  Float* x_float = new Float[sz];

  uint64_t* y = new uint64_t[sz];
  IntFp* y_Fp = new IntFp[sz];
  Integer* y_f = new Integer[sz];
  Float* y_float = new Float[sz];


  for(int i = 0; i < sz; i++){
    x[i] = rand() % PR;
    x_Fp[i] = IntFp(x[i], ALICE);
    x_f[i] = Integer(62, 0, ALICE);

    y[i] = rand() % PR;
    y_Fp[i] = IntFp(y[i], ALICE);
    y_f[i] = Integer(62, 0, ALICE);
  }

  start = clock_start();
  arith2bool<BoolIO<NetIO>>(x_f, x_Fp, sz);
  arith2bool<BoolIO<NetIO>>(y_f, y_Fp, sz);
  cout << "Comm after A2B: " << ios[0]->counter / sz << " bytes \n";

  for(int i = 0; i < sz; i++){
    x_float[i] = Int62ToFloat(x_f[i], 16);
    y_float[i] = Int62ToFloat(y_f[i], 16);
    
    // relu
    Bit c = x_float[i].less_than(Float(0, PUBLIC));

    x_f[i] = Integer(62, &c, ALICE);
  }
  cout << "Comm after ReLU: " << ios[0]->counter / sz << " bytes \n";

  bool2arith<BoolIO<NetIO>>(x_Fp, x_f, sz);
  cout << "Comm after B2A: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_bool<BoolIO<NetIO>>();
  finalize_zk_arith<BoolIO<NetIO>>();

  auto timecrypto = time_from(start);
  cout << "Avg. time for float comparison for " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_plain_prot();
}



void float_inner_product(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ FLOATING POINT INNER PRODUCT  ------\n";
  
  auto start = clock_start();
  // setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;
  
  Float* a_float = new Float[sz];
  Float* b_float = new Float[sz];

  for(int i = 0; i < sz; i++){
    a_float[i] = Float(1.141, ALICE);
    b_float[i] = Float(2.712, ALICE);
  }

  // inner product
  ios[0]->counter = 0;
  start = clock_start();
  for(int i = 0; i < sz; i++){
    b_float[i] = b_float[i] + (a_float[i]);
    // a_float[i] = b_float[i] * (a_float[i]);
  }

  finalize_plain_prot();

  auto timecrypto = time_from(start);
  cout << "Avg. time for addition of " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
}

void float_add_mult(BoolIO<NetIO> *ios[threads], int party){
  cout << "\n\n------ FLOATING POINT COMPARISON  ------\n";
  
  auto start = clock_start();
  // setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  int sz = test_n;
  
  Float* a_float = new Float[sz];
  Float* b_float = new Float[sz];

  for(int i = 0; i < sz; i++){
    a_float[i] = Float(1.141, ALICE);
    b_float[i] = Float(2.712, ALICE);
  }

  // addition
  ios[0]->counter = 0;
  start = clock_start();
  for(int i = 0; i < sz; i++){
    b_float[i] = b_float[i] + (a_float[i]);
    // a_float[i] = b_float[i] * (a_float[i]);
  }

  finalize_plain_prot();

  auto timecrypto = time_from(start);
  cout << "Avg. time for addition of " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";


  setup_plain_prot(false, "");

  a_float = new Float[sz];
  b_float = new Float[sz];

  for(int i = 0; i < sz; i++){
    a_float[i] = Float(1.141, ALICE);
    b_float[i] = Float(2.712, ALICE);
  }

  // mult
  ios[0]->counter = 0;
  start = clock_start();
  for(int i = 0; i < sz; i++){
    b_float[i] = b_float[i] * (a_float[i]);
    // a_float[i] = b_float[i] * (a_float[i]);
  }

  finalize_plain_prot();

  timecrypto = time_from(start);
  cout << "Avg. time for multiplication of " << test_n << " instances: " << timecrypto / sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";
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

  // seq_relu_fxp(ios, party);
  // seq_relu_float(ios, party);

  // relu_test(ios, party);
  float_comp(ios, party);
  // float_add_mult(ios, party);


  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
