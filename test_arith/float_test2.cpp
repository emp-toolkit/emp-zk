#include "emp-wolverine-fp/floats.h"
#include "emp-wolverine-fp/emp-wolverine-fp.h"
#include <iostream>
#include <string>
using namespace emp;
using namespace std;
int port, party;
const int threads = 5;

float int62tofloat(int64_t a, int s) {
  if (a > ((1LL << 60) - 1))
    a = a - ((1LL << 61) - 1);
  if (a == ((1LL << 60) - 1))
    return 0;
  return (a + 0.0) / (0.0 + (1LL << s));
}

void perf_test() {
  int SCALE = 16;
  int64_t size = 100000;
  string separation_line = "----------------------------------------------------------------------";

  {
    auto start = clock_start();
    for (int64_t i = 0; i < size; i++) {
      Integer a(62, 123456, ALICE);
      Int62ToFloat(a, SCALE);
    }
    double elpased = time_from(start);
    cout << "Int62ToFloat --> size:" << size << ", elapse(us):" << elpased
         << ", avg(us):" << elpased / size << endl;
    cout << separation_line << endl;
  }
  {
    int64_t size = 100000;
    auto start = clock_start();
    for (int64_t i = 0; i < size; i++) {
      Float f1(1.23456789, ALICE);
      FloatToInt62(f1, SCALE);
    }
    double elpased = time_from(start);
    cout << "FloatToInt62 --> size:" << size << ", elapse(us):" << elpased
         << ", avg(us):" << elpased / size << endl;
    cout << separation_line << endl;
  }
  {
    int64_t size = 100000;
    auto start = clock_start();
    Integer a(62, 123456, ALICE);
    for (int64_t i = 0; i < size; i++) {
      Int62ToFloat(a, SCALE);
    }
    double elpased = time_from(start);
    cout << "Int62ToFloat --> size:" << size << ", elapse(us):" << elpased
         << ", avg(us):" << elpased / size << endl;
    cout << separation_line << endl;
  }
  {
    int64_t size = 100000;
    auto start = clock_start();
    Float f1(1.23456789, ALICE);
    for (int64_t i = 0; i < size; i++) {
      FloatToInt62(f1, SCALE);
    }
    double elpased = time_from(start);
    cout << "FloatToInt62 --> size:" << size << ", elapse(us):" << elpased
         << ", avg(us):" << elpased / size << endl;
    cout << separation_line << endl;
  }
}

int main(int argc, char** argv) {
  parse_party_and_port(argv, &party, &port);

  NetIO* ios[threads + 1];
  for (int i = 0; i < threads + 1; ++i)
    ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);

  setup_boolean_zk<NetIO>(ios, threads, party);
  setup_fp_zk<NetIO>(ios, threads, party);

  ///
  Integer a(62, (1ULL << 61) - 2, ALICE);
  cout << "Int62ToFloat:" << Int62ToFloat(a, 0).reveal<double>(PUBLIC) << endl;
  perf_test();
  ///

  finalize_boolean_zk<NetIO>(party);
  finalize_fp_zk<NetIO>();

  for (int i = 0; i < threads + 1; ++i)
    delete ios[i];

  return 0;
}

//
