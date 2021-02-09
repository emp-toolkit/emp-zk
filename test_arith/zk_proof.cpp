#include "emp-zk-arith/emp-zk-arith.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 5;

void test_circuit_zk(NetIO *ios[threads], int party) {
	int test_n = 1024*1024*64;

	std::cout << "performance test" << std::endl;
	auto start = clock_start();
	setup_zk_bool<NetIO>(ios, threads, party);
	setup_zk_arith<NetIO>(ios, threads, party);
	auto timesetup = time_from(start);
	cout << "\tsetup: " << timesetup*1000 <<" "<<party<<" "<<endl;

	ios[0]->sync();
	__uint128_t ar = 2, br = 3, cr = 4;
	IntFp a((uint64_t)ar, ALICE);
	IntFp b((uint64_t)br, ALICE);
	IntFp c((uint64_t)cr, PUBLIC);

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

	c.reveal(cr);
	sync_zk_bool<NetIO>();
	auto timeuse = time_from(start);
	cout << "\taverage time per gate: " << (timeuse+timesetup)/test_n*1000<<" ns "<<party<<endl;

	std::cout << "correctness check test" << std::endl;
	srand(time(NULL));
	int sz = 10000;
	uint64_t *d = new uint64_t[sz];
	uint64_t *e = new uint64_t[sz];
	IntFp *di = new IntFp[sz];
	IntFp *ei = new IntFp[sz];
	IntFp *fi = new IntFp[sz];
	IntFp PRi = IntFp(PR-1, PUBLIC);
	for(int i = 0; i < sz; ++i) {
		d[i] = rand() % PR;
		e[i] = rand() % PR;

		di[i] = IntFp(d[i], ALICE);
		ei[i] = IntFp(e[i], ALICE);

		d[i] = (d[i] * e[i]) % PR;
		di[i] = di[i] * ei[i];

		e[i] = (d[i] + e[i]) % PR;
		ei[i] = di[i] + ei[i];

		fi[i] = IntFp(e[i], ALICE);
		fi[i] = PRi * fi[i];
		fi[i] = ei[i] + fi[i];
	}
	std::cout << "\treveal" << std::endl;
	uint64_t *dd = new uint64_t[sz];
	batch_reveal(di, dd, sz);
	if(memcmp(dd, d, sz*sizeof(uint64_t)) != 0) error("reveal fails");
	std::cout << "\treveal check" << std::endl;
	batch_reveal_check(ei, e, sz);
	std::cout << "\treveal check zero" << std::endl;
	batch_reveal_check_zero(fi, sz);

	std::cout << std::endl;

	finalize_zk_bool<NetIO>();

	delete[] d;
	delete[] e;
	delete[] di;
	delete[] fi;
	delete[] ei;
	delete[] dd;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ circuit zero-knowledge proof test ------------" << std::endl << std::endl;

	test_circuit_zk(ios, party);

	for(int i = 0; i < threads; ++i)
		delete ios[i];
	return 0;
}
