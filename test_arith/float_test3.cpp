#include "emp-zk-arith/floats.h"
#include <iostream>
using namespace emp;
using namespace std;

float int62tofloat(int64_t a, int s) {
  if (a > ((1LL << 60) - 1))
    a = a - ((1LL << 61) - 1);
  if (a == ((1LL << 60) - 1))
    return 0;
  return (a + 0.0) / (0.0 + (1LL << s));
}

int main() {
  setup_plain_prot(false, "");
  int gate = 0;

  //for test
  cout << "==================\n";
  Float sa0(1.59699e-12, ALICE);
  Float sa1(6.0611e-19, ALICE);
  Float sa2(6.52081e-08, ALICE);
  Float sa3(7.36336e-07, ALICE);
  Float sa4(3.63666e-18, ALICE);
  Float sa5(0.999999, ALICE);
  Float sa6(1., ALICE);
  Float sa7(1.38021e-13, ALICE);

  cout << FloatToInt62(sa0, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << FloatToInt62(sa1, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << FloatToInt62(sa2, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << FloatToInt62(sa3, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << FloatToInt62(sa4, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << FloatToInt62(sa5, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(65535)
  cout << FloatToInt62(sa6, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(65535)
  cout << FloatToInt62(sa7, 16).reveal<uint64_t>(PUBLIC) << "\t\t"; //(0)
  cout << "\n==================\n";
  //end test

  Integer a(62, (1ULL << 61) - 2, ALICE);
  cout << Int62ToFloat(a, 0).reveal<double>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  a = Integer(62, (1ULL << 60) - 5, ALICE);
  cout << Int62ToFloat(a, 20).reveal<double>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  a = Integer(62, (1ULL << 20) - 2, ALICE);
  cout << Int62ToFloat(a, 0).reveal<double>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  a = Integer(62, (1ULL << 61) - 3, ALICE);
  cout << Int62ToFloat(a, 0).reveal<double>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  a = Integer(62, (1ULL << 60) - 1, ALICE);
  cout << Int62ToFloat(a, 0).reveal<double>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  cout << "----------------------------------------\n";
  Float b(1.09951e+12, ALICE);
  cout << FloatToInt62(b, 20).reveal<uint64_t>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  b = Float(-1, ALICE);
  cout << FloatToInt62(b, 20).reveal<uint64_t>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  b = Float(0, ALICE);
  cout << FloatToInt62(b, 20).reveal<uint64_t>(PUBLIC) << "\t\t";
  cout << CircuitExecution::circ_exec->num_and() - gate << endl;
  gate = CircuitExecution::circ_exec->num_and();

  cout << "----------------------------------------\n";
  PRG prg;
  for (int i = 0; i < 100; ++i) {
    uint64_t t = 0;
    prg.random_data(&t, 8);
    t = t & ((1ULL << 61) - 1);
    Integer a(62, t, PUBLIC);
    cout << a.reveal<uint64_t>(PUBLIC) << "\t";
    Float f = Int62ToFloat(a, 20);
    cout << f.reveal<double>(PUBLIC) << "\t";
    Integer b = FloatToInt62(f, 20);
    cout << b.reveal<uint64_t>(PUBLIC) << "\t";
    cout << int62tofloat(t, 20) << "\t";
    cout << f.reveal<double>(PUBLIC) / int62tofloat(t, 20) << "\t";
    cout << (b.reveal<uint64_t>(PUBLIC) + 0.0) / (0.0 + t) << "\n";
  }

  finalize_plain_prot();
  return 0;
}