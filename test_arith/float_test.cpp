#include "emp-wolverine-fp/floats.h"
#include <iostream>
using namespace emp;
using namespace std;


float int62tofloat(int64_t a, int s) {
	if(a > ((1LL<<60)-1))
		a = a - ((1LL<<61)-1);
	if(a ==  ((1LL<<60)-1))
		return 0;
	return (a+0.0)/(0.0 + (1LL<<s));
}

int main() {
	setup_plain_prot(false, "");
	int gate = 0;
	
	Integer a(62, (1ULL<<61)-2, ALICE);
	cout << Int62ToFloat(a,0).reveal<double>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	a =Integer(62, (1ULL<<60)-5, ALICE);
	cout << Int62ToFloat(a,20).reveal<double>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	a =Integer(62, (1ULL<<20)-2, ALICE);
	cout << Int62ToFloat(a,0).reveal<double>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	a = Integer(62, (1ULL<<61)-3, ALICE);
	cout << Int62ToFloat(a,0).reveal<double>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	a = Integer(62, (1ULL<<60)-1, ALICE);
	cout << Int62ToFloat(a,0).reveal<double>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	cout <<"----------------------------------------\n";
	Float b(1.09951e+12, ALICE);
	cout << FloatToInt62(b,20).reveal<uint64_t>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();
	
	b = Float(-1, ALICE);
	cout << FloatToInt62(b,20).reveal<uint64_t>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	b = Float(0, ALICE);
	cout << FloatToInt62(b,20).reveal<uint64_t>(PUBLIC)<<"\t\t";
	cout << CircuitExecution::circ_exec->num_and() - gate<<endl;
	gate = CircuitExecution::circ_exec->num_and();

	cout <<"----------------------------------------\n";
	PRG prg;
	for(int i = 0; i < 100; ++i) {
		uint64_t t = 0; prg.random_data(&t, 8);
		t = t & ((1ULL<<61)-1);
		Integer a(62, t, PUBLIC);
		cout << a.reveal<uint64_t>(PUBLIC)<<"\t";
		Float f = Int62ToFloat(a, 20);
		cout << f.reveal<double>(PUBLIC)<<"\t";
		Integer b = FloatToInt62(f, 20);
		cout << b.reveal<uint64_t>(PUBLIC)<<"\t";
		cout << int62tofloat(t, 20)<<"\t";
		cout << f.reveal<double>(PUBLIC) / int62tofloat(t, 20)<<"\t";
		cout << (b.reveal<uint64_t>(PUBLIC)+0.0) / (0.0+t)<<"\n"; 
	}
	

	finalize_plain_prot();
	return 0;
}