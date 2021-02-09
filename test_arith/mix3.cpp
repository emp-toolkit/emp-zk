#include "emp-tool/emp-tool.h"
#include "emp-wolverine-bool/emp-wolverine-bool.h"
#include "emp-wolverine-fp/emp-wolverine-fp.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = N_THREADS;

void test_mix_circuit(NetIO *ios[threads], int party) {
	srand(time(NULL));
	int sz = 110000;
	uint64_t *a = new uint64_t[sz];
	for(int i = 0; i < sz; ++i)
		a[i] = rand() % PR;

	setup_boolean_zk<NetIO>(ios, threads, party);
	setup_fp_zk<NetIO>(ios, threads, party);

	IntFp *x = new IntFp[sz];
	/* normal input */
	/*for(int i = 0; i < sz; ++i)
		x[i] = IntFp(a[i], ALICE);*/
	/* batch input */
	batch_feed(x, a, sz);

	sync_boolean_zk();

	Integer *y = new Integer[sz];
	for(int i = 0; i < sz; ++i)
		y[i] = Integer(62, a[i], ALICE);

	Integer PR_bl = Integer(62, PR, PUBLIC);

	sync_boolean_zk();

	/*for(int k = 0; k < 2; ++k) {
		for(int i = 0; i < 3; ++i) {
			for(int j = i; j < sz-3; j+=3) {
				y[j+2] = y[j+1] + y[j];
				y[j+2] = y[j+2].select(y[j+2].bits[61], y[j+2] - PR_bl);
				x[j+2] = bool2arith(y[j+2]);

				x[j] = x[j+1] + x[j+2];
				y[j] = arith2bool(x[j]);

				a[j+2] = a[j+1] + a[j];
				if(a[j+2] > PR) a[j+2] -= PR;
				a[j] = a[j+1] + a[j+2];
				if(a[j] > PR) a[j] -= PR;
			}
		}
	}

	for(int i = 0; i < sz; ++i) {
		Bit ret = y[i].equal(Integer(62, a[i], PUBLIC));
		if(ret.reveal<bool>(PUBLIC) != 1) {
			std::cout << "wrong boolean" << std::endl;
			break;
		}
	}
	std::cout << "end check boolean" << std::endl;

	batch_reveal_check(x, a, sz);
	std::cout << "end check arithmetic" << std::endl;*/

	auto start = clock_start();
	for(int k = 0; k < 4; ++k) {
		for(int i = 0; i < 3; ++i) {
			for(int j = i; j < sz-3; j+=3) {
				y[j+2] = y[j+1] + y[j];
				y[j+2] = y[j+2].select(y[j+2].bits[61], y[j+2] - PR_bl);

				a[j+2] = a[j+1] + a[j];
				if(a[j+2] > PR) a[j+2] -= PR;
			}
			bool2arith<NetIO>(x, y, sz);

			for(int j = i; j < sz-3; j+=3) {
				x[j] = x[j+1] + x[j+2];
				a[j] = a[j+1] + a[j+2];
				if(a[j] > PR) a[j] -= PR;
			}
			arith2bool<NetIO>(y, x, sz);
		}
	}

	int incorrect_cnt = 0;
	for(int i = 0; i < sz; ++i) {
		Bit ret = y[i].equal(Integer(62, a[i], PUBLIC));
		if(ret.reveal<bool>(PUBLIC) != 1)
			incorrect_cnt++;
	}
	if(incorrect_cnt) std::cout << "incorrect boolean: " << incorrect_cnt << std::endl;
	std::cout << "end check boolean" << std::endl;

	batch_reveal_check(x, a, sz);
	std::cout << "end check arithmetic" << std::endl;

	finalize_boolean_zk<NetIO>(party);
	finalize_fp_zk<NetIO>();

	double tt = time_from(start);
	std::cout << "conversion: " << tt/(4*3*2)/sz << std::endl;

	delete[] a;
	delete[] x;
	delete[] y;
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
