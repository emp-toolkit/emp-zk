#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

void test_circuit_zk(BoolIO<NetIO> *ios[threads], int party) {
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  
  Float* a = new Float[1000000];
  Float* b = new Float[1000000];

  ios[0]->counter = 0;
  for(int i = 0; i < 1000000; i++){
  a[i] = Float(3.14, ALICE);
  b[i] = Float(3.56, ALICE);
  }

  cout << "Comm: " << ios[0]->counter << " bytes\n";
  auto start = clock_start();
  for(int i =0; i < 1000000; i++){
    Float c = (a[i] * b[i]);//.reveal<double>(PUBLIC) << " ";
  }

  bool cheat = finalize_zk_bool<BoolIO<NetIO>>();
  if (cheat)
    error("cheat!\n");

  auto timecrypto = time_from(start);
  cout << "Time: " << timecrypto/1000000 << " us\n";
  cout << "Comm: " << ios[0]->counter/1000000 << " bytes\n";
}

void test_int_field(BoolIO<NetIO> *ios[threads], int party) {
  // setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

  uint64_t a_clt = 4.52 * (1 << 16);
  uint64_t b_clt = PR - (6.31 * (1 << 16));

  Integer a(61, a_clt, PUBLIC);
  Integer b(61, b_clt, PUBLIC);

  IntFp a_Fp = new IntFp(a.reveal<uint64_t>(), PUBLIC);
  IntFp b_Fp = new IntFp(b.reveal<uint64_t>(), PUBLIC);

  cout << "INT COMP: \n";
  cout << a.reveal<uint64_t>() << "\n";
  cout << b.reveal<uint64_t>() << "\n";

  b = a * b;
  cout << b.reveal<uint64_t>() << "\n";


  cout << "FIELD: \n";
  cout << a_Fp.reveal() << "\n";
  cout << b_Fp.reveal() << "\n";

  b_Fp = a_Fp * b_Fp;

  cout << "FIELD: \n";
  cout << a_Fp.reveal() << "\n";
  cout << b_Fp.reveal() << "\n";
}


void test_int30_field(BoolIO<NetIO> *ios[threads], int party) {
  // setup_plain_prot(false, "");
  setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
  setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

  uint32_t a_clt = 4.52 * (1 << 16);
  uint32_t b_clt = PR - (6.31 * (1 << 16));

  Integer a(30, a_clt, PUBLIC);
  Integer b(30, b_clt, PUBLIC);

  IntFp a_Fp = new IntFp(a.reveal<uint64_t>(), PUBLIC);
  IntFp b_Fp = new IntFp(b.reveal<uint64_t>(), PUBLIC);

  cout << "INT COMP: \n";
  cout << a.reveal<uint64_t>() << "\n";
  cout << b.reveal<uint64_t>() << "\n";

  cout << "\nFIELD: \n";
  cout << a_Fp.reveal() << "\n";
  cout << b_Fp.reveal() << "\n";

  b_Fp = a_Fp * b_Fp;

  cout << "\nFIELD: \n";
  cout << a_Fp.reveal() << "\n";
  cout << b_Fp.reveal() << "\n";
}


int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);
  BoolIO<NetIO> *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new BoolIO<NetIO>(
        new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
        party == ALICE);

  // test_circuit_zk(ios, party);
  // test_int_field(ios, party);
  test_int30_field(ios, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i]->io;
    delete ios[i];
  }
  return 0;
}
