#include <emp-zk/emp-zk.h>
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 5;


void test_circuit_zk(NetIO *ios[threads+1], int party, int input_sz_lg) {

	long long test_n = 1<<input_sz_lg;
	auto start = clock_start();
	setup_zk_arith<NetIO>(ios, threads, party);
	auto timesetup = time_from(start);
	cout << "time for setup: " << timesetup*1000 <<" "<<party<<" "<<endl;

	ios[0]->sync();
	start = clock_start();
	__uint128_t ar = 2, br = 3, cr = 4;
	IntFp a((uint64_t)ar, ALICE);
	IntFp b((uint64_t)br, ALICE);
	IntFp c((uint64_t)cr, PUBLIC);
	cout << "time for input in total: " << time_from(start)*1000<<" "<<party<<" "<<endl;

	for(int i = 0; i < test_n; ++i) {
		br = (br + ar)%pr;
		ar = (br * ar)%pr;
	}
	cr = (ar * br)%pr;
	cr = (cr + ar)%pr;

	start = clock_start();
	for(int i = 0; i < test_n; ++i) {
		b = b + a;
		a = b * a;
	}
	c = a * b;
	c = c + a;

	bool ret = c.reveal(cr);
	auto timeuse = time_from(start);
	cout << test_n << "\t" << (timeuse+timesetup) << "\t" << party << " " << ret << endl;
	std::cout << std::endl;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads+1];
	for(int i = 0; i < threads+1; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;;

	int num = atoi(argv[3]);
	test_circuit_zk(ios, party, num);

	for(int i = 0; i < threads+1; ++i)
		delete ios[i];
	return 0;
}
