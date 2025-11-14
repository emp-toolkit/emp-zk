#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/extensions/floats.h"
#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int test_n = 1;

void seq_mult_float(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOATING POINT MULTIPLICATION ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  ios[0]->counter = 0;
  srand(time(NULL));
  int sz = test_n;
  // cleartext
  float *d = new float[sz];
  float *e = new float[sz];
  float *f = new float[sz];

  // auth
  Float *di = new Float[sz];
  Float *ei = new Float[sz];
  Float *fi = new Float[sz];
  Integer *fint = new Integer[sz];
  IntFp *ff = new IntFp[sz];

  for (int i = 0; i < sz; i++){
    d[i] = rand();
    e[i] = rand();

    di[i] = Float(d[i], ALICE);
    ei[i] = Float(e[i], ALICE);
    fi[i] = Float(e[i], ALICE);

    f[i] = (d[i] * e[i]);
  }

  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    fi[i] = di[i] * ei[i];
    fint[i] = FloatToInt62(fi[i], 16);
    ff[i] = IntFp(fint[i].reveal<uint64_t>(), ALICE);

    if(fi[i].reveal<double>() != (double) f[i]){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  uint64_t *dd = new uint64_t[sz];
  batch_reveal(ff, (uint64_t*) d, sz);

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_plain_prot();

  finalize_zk_arith<BoolIO<NetIO>>();
}



void seq_mult_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "------ FIXED POINT MULTIPLICATION ------\n";

  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  ios[0]->counter = 0;
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

  for (int i = 0; i < sz; i++){
    d[i] = rand() % PR;
    e[i] = rand() % PR;

    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);
    fi[i] = IntFp(e[i], ALICE);

    f[i] = (d[i] * e[i]) % PR;
  }

  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    fi[i] = di[i] * ei[i];
    if(fi[i].reveal(f[i]) != 1){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  uint64_t *dd = new uint64_t[sz];
  batch_reveal(di, dd, sz);
  if (memcmp(dd, d, sz * sizeof(uint64_t)) != 0)
    error("reveal fails");

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_arith<BoolIO<NetIO>>();
}





void seq_add_float(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOATING POINT ADDITION ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  setup_plain_prot(false, "");
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


  srand(time(NULL));
  int sz = test_n;
  // cleartext
  float *d = new float[sz];
  float *e = new float[sz];
  float *f = new float[sz];

  // auth
  Float *di = new Float[sz];
  Float *ei = new Float[sz];
  Float *fi = new Float[sz];

  for (int i = 0; i < sz; i++){
    d[i] = rand();
    e[i] = rand();

    di[i] = Float(d[i], ALICE);
    ei[i] = Float(e[i], ALICE);
    fi[i] = Float(e[i], ALICE);

    f[i] = (d[i] + e[i]);
  }

  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    fi[i] = di[i] + ei[i];
    if(fi[i].reveal<double>() != (double) f[i]){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;

  finalize_plain_prot();

  finalize_zk_arith<BoolIO<NetIO>>();
}




void seq_add_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIXED POINT ADDITION ------\n";

  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  ios[0]->counter = 0;
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

  for (int i = 0; i < sz; i++){
    d[i] = rand() % PR;
    e[i] = rand() % PR;

    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);
    fi[i] = IntFp(e[i], ALICE);

    f[i] = (d[i] + e[i]) % PR;
  }

  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    fi[i] = di[i] + ei[i];
    if(fi[i].reveal(f[i]) != 1){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  uint64_t *dd = new uint64_t[sz];
  batch_reveal(di, dd, sz);
  if (memcmp(dd, d, sz * sizeof(uint64_t)) != 0)
    error("reveal fails");

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_arith<BoolIO<NetIO>>();
}



void seq_comp_float(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FLOATING POINT COMPARISON ------\n";
  
  auto start = clock_start();
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
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
  Bit *fi = new Bit[sz];

  for (int i = 0; i < sz; i++){
    d[i] = rand();
    e[i] = rand();

    di[i] = Float(d[i], ALICE);
    ei[i] = Float(e[i], ALICE);
    fi[i] = Bit(0, ALICE);

    f[i] = (d[i] <= e[i]);
  }

  start = clock_start();
  // sz operations
  for (int i = 0; i < sz; i++) {
    fi[i] = di[i].less_equal(ei[i]);
    if(fi[i].reveal<bool>() != (bool) f[i]){
      cout << i << ": " << "reveal fail\n";
      exit(0);
    }
  }

  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;

  finalize_plain_prot();

  finalize_zk_arith<BoolIO<NetIO>>();
}

void seq_comp_fxp(BoolIO<NetIO> *ios[threads], int party) {
  cout << "\n\n------ FIXED POINT COMPARISON ------\n";

  auto start = clock_start();
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
  auto timesetup = time_from(start);
  cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;

  ios[0]->counter = 0;
  srand(time(NULL));
  int sz = test_n;
  // cleartext
  uint64_t *d = new uint64_t[sz];
  uint64_t *e = new uint64_t[sz];
  bool *f = new bool[sz];

  // auth
  IntFp *di = new IntFp[sz];
  IntFp *ei = new IntFp[sz];

  Integer *dint = new Integer[sz];
  Integer *eint = new Integer[sz];
  Bit *fi = new Bit[sz];

  Integer PR_bl = Integer(62, PR, PUBLIC);

  for (int i = 0; i < sz; i++){
    d[i] = rand() % PR;
    e[i] = rand() % PR;

    di[i] = IntFp(d[i], ALICE);
    ei[i] = IntFp(e[i], ALICE);
    dint[i] = Integer(62, d[i], ALICE);
    eint[i] = Integer(62, e[i], ALICE);

    // negative handling
    // dint[i] = dint[i].select(dint[i].bits[61], dint[i] - PR_bl);
    // eint[i] = eint[i].select(eint[i].bits[61], eint[i] - PR_bl);

    f[i] = (d[i] >= e[i]);
    fi[i] = Bit(0, PUBLIC);
  }

  sync_zk_bool<BoolIO<NetIO>>();

  start = clock_start();
  // arith2bool<BoolIO<NetIO>>(dint, di, sz);
  // arith2bool<BoolIO<NetIO>>(eint, ei, sz);

  // sz operations
  for (int i = 0; i < sz; i++) {
    Bit ge = dint[i].geq(Integer(62, d[i], PUBLIC));
    if(!ge.reveal<bool>(PUBLIC)){
      cout << i << ": " << "reveal fail\n";
      cout << "d = " << d[i] << ", e = " << e[i] << ", dint = " << dint[i].reveal<uint64_t>()
           << ", eint = " << eint[i].reveal<uint64_t>() << ", f = " << f[i] << ", ge = " << ge.reveal<bool>() << "\n";
      
      cout << "dint = " << dint[i].reveal<string>() << "\n";
      cout << "eint = " << eint[i].reveal<string>() << "\n";
      
      //  exit(0);
    }
  }

  // batch_reveal_check(di, d, sz);
  auto timecrypto = time_from(start);

  cout << "Avg. time for " << sz << " operations = " << timecrypto /  sz << " μs; PARTY =" << party << " " << endl;
  cout << "Comm: " << ios[0]->counter / sz << " bytes \n";

  finalize_zk_arith<BoolIO<NetIO>>();
  finalize_zk_bool<BoolIO<NetIO>>();
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
  // seq_mult_float(ios, party);

  // seq_add_fxp(ios, party);
  // seq_add_float(ios, party);

  seq_comp_fxp(ios, party);
  seq_comp_float(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
