#include "emp-zk-arith/emp-zk-arith.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 5;

void test_input_speed(NetIO **ios, int party, int sz) {
	std::cout << "input size: " << sz << std::endl;
	srand(time(NULL));
	uint64_t *a = new uint64_t[sz];
	for(int i = 0; i < sz; ++i)
		a[i] = rand() % PR;

	setup_zk_bool<NetIO>(ios, threads, party);
	setup_zk_arith<NetIO>(ios, threads, party);

	IntFp *x = new IntFp[sz];

	/* normal input */
	auto start = clock_start();
	for(int i = 0; i < sz; ++i)
		x[i] = IntFp(a[i], ALICE);
	sync_zk_bool<NetIO>();
	double tt = time_from(start);
	std::cout << "normal input average time: " << tt*1000/sz << " ns per element" << std::endl;

	/* batch input */
	start = clock_start();
	batch_feed(x, a, sz);
	sync_zk_bool<NetIO>();
	tt = time_from(start);
	std::cout << "batch input average time: " << tt*1000/sz << " ns per element" << std::endl;

	finalize_zk_bool<NetIO>(party);
	finalize_zk_arith<NetIO>();

	delete[] a;
	delete[] x;	
}

/*void test_circuit_zk(NetIO *ios[threads+1], int party) {

	long long input_sz = 1048576;
	while(input_sz < 1000000000LL) {
		auto start = clock_start();
		setup_fp_zk<NetIO, threads>(ios, party);
		IntFp *a = new IntFp[input_sz];
		for(int i = 0; i < input_sz; ++i)
			a[i] = IntFp((uint64_t)i, ALICE);
		cout << input_sz << "\t" << time_from(start)<< endl;
		cout << a[0].reveal(0) <<endl;
		delete[] a;
		input_sz = input_sz * 4;
	}

	long long unit = input_sz / 4;
	input_sz = unit * 2;
	while(input_sz < 1100000000LL) {
		auto start = clock_start();
		setup_fp_zk<NetIO, threads>(ios, party);
		int round = input_sz / unit;
		IntFp **a = (IntFp**)malloc(round*sizeof(IntFp*));
		for(int i = 0; i < round; ++i) {
			a[i] = new IntFp[unit];
			for(int j = 0; j < unit; ++j)
				a[i][j] = IntFp((uint64_t)j, ALICE);
		}
		cout << input_sz << "\t" << time_from(start)<< endl;
		cout << a[0][0].reveal(0) <<endl;
		for(int i = 0; i < round; ++i)
			delete[] a[i];
		free(a);
		input_sz *= 2;
	}
}*/

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads+1];
	for(int i = 0; i < threads+1; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;;

	test_input_speed(ios, party, 1000000);
	test_input_speed(ios, party, 10000000);
	test_input_speed(ios, party, 40000000);
	test_input_speed(ios, party, 80000000);

	for(int i = 0; i < threads+1; ++i)
		delete ios[i];
	return 0;
}
