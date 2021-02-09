#include "emp-zk-bool/emp-zk-bool.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

void test_circuit_zk(NetIO *ios[threads], int party, int log_trail) {

	long long input_sz = 1<<log_trail;
	if(input_sz < 100000000LL) {
		auto start = clock_start();
		setup_zk_bool<NetIO>(ios, threads, party);
		Integer *a = new Integer[input_sz/32];
		for(int i = 0; i < input_sz/32; ++i)
			a[i] = Integer(32, i, ALICE);

		a[0][0].reveal<bool>(PUBLIC);
		finalize_zk_bool<NetIO>(party);
		double timeused = time_from(start);
		cout << input_sz << "\t" << timeused << endl;
		delete[] a;
	} else {
		long long unit = 1 << 24; 
		auto start = clock_start();
		setup_zk_bool<NetIO>(ios, threads, party);
		int round = input_sz / unit;
		Integer **a = (Integer**)malloc(round*sizeof(Bit*));
		for(int i = 0; i < round; ++i) {
			a[i] = new Integer[unit];
			for(int j = 0; j < unit/32; ++j)
				a[i][j] = Integer(32, j, ALICE);
		}
		a[0][0][0].reveal<bool>(PUBLIC);
		bool cheated = finalize_zk_bool<NetIO>(party);
		if(cheated)error("cheated\n");
		double timeused = time_from(start);
		cout << input_sz << "\t" << timeused << endl;
		for(int i = 0; i < 8; ++i)
			delete[] a[i];
		free(a);
	}
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;;

	int num = atoi(argv[3]);
	test_circuit_zk(ios, party, num);

	for(int i = 0; i < threads; ++i)
		delete ios[i];
	return 0;
}
