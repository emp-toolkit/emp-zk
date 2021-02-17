#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
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

	setup_zk_bool<NetIO>(ios, threads, party);
	setup_zk_arith<NetIO>(ios, threads, party);
	IntFp a1(a, ALICE);
	IntFp a2(b, ALICE);
	a1 = a1 + a2;

	Integer b1(62, c, ALICE);
	Integer b2;
	b2 = arith2bool<NetIO>(a1);
	b1 = b1 + b2;
	Integer b3(62, d, PUBLIC);

	IntFp a3;
	a3 = bool2arith<NetIO>(b3);
	sync_zk_bool<NetIO>();
	cout << a3.reveal(d) << endl;	

	finalize_zk_bool<NetIO>();
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads+1];
	for(int i = 0; i < threads+1; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;;

	test_mix_circuit(ios, party);

	for(int i = 0; i < threads+1; ++i)
		delete ios[i];
	return 0;
}
