#include "emp-zk-bool/emp-zk-bool.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

void test_circuit_zk(NetIO *ios[threads], int party) {
	setup_zk_bool<NetIO>(ios, threads, party);
	Integer a(32, 3, ALICE);
	Integer b(32, 2, ALICE);
	cout << (a-b).reveal<uint32_t>(PUBLIC)<<endl;

	finalize_zk_bool<NetIO>(party);
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	test_circuit_zk(ios, party);

	for(int i = 0; i < threads; ++i)
		delete ios[i];
	return 0;
}
