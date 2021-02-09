#include "emp-tool/emp-tool.h"
#include "emp-zk-bool/emp-zk-bool.h"
#include "emp-zk-arith/emp-zk-arith.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 5;

void test_mix_circuit(NetIO *ios[threads+1], int party) {
	srand(time(NULL));
	uint64_t a = rand()%p;
	uint64_t b = rand()%p;
	uint64_t c = rand()%p;
	uint64_t d = ((a+b)%p+c)%p;

	setup_boolean_zk<NetIO>(ios, threads, party); 		// sometime stuck
	setup_fp_zk<NetIO>(ios, threads, party);

	EdaBits *conv;
	if(party == ALICE) {
		conv = new EdaBits(ALICE, threads, ios, ((ZKFpExecPrv<NetIO>*)(ZKFpExec::zk_exec))->ostriple->vole);
	} else {
		conv = new EdaBits(BOB, threads, ios, ((ZKFpExecVer<NetIO>*)(ZKFpExec::zk_exec))->ostriple->vole);
		conv->install_boolean(((ZKBoolCircExecVer*)CircuitExecution::circ_exec)->delta);
	}

	IntFp a1(a, ALICE);
	IntFp a2(b, ALICE);
	a1 = a1 + a2;

	Integer b1(62, c, ALICE);
	Integer b2;
	//b2 = arith2bool(a1);
	b2 = conv->arith2bool(a1.value);
	b1 = b1 + b2;
	Integer b3(62, d, PUBLIC);
	Bit ret = (b1.equal(b3));
	//ret = !ret;
	cout << ret.reveal<bool>(PUBLIC) << endl;

	IntFp a3;
	//a3 = bool2arith(b3);
	a3.value = conv->bool2arith(b3);
	cout << a3.reveal(d) << endl;	

	delete conv;

	finalize_boolean_zk<NetIO>(party);
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;;

	test_mix_circuit(ios, party);

	for(int i = 0; i < threads; ++i)
		delete ios[i];
	return 0;
}
