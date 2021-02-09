#include "emp-zk-bool/emp-zk-bool.h"
#include <iostream>
#include "emp-tool/emp-tool.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

void test_polynomial(NetIO *ios[threads], int party) {
	srand(time(NULL));
	int sz = 100000;
	int repeat = 10000;
	bool *coeff = new bool[sz+1];
	bool *witness = new bool[2*sz];
	memset(witness, 0, 2*sz*sizeof(bool));

	setup_zk_bool<NetIO>(ios, threads, party);
	sync_zk_bool<NetIO>();

	Bit *x = new Bit[2*sz];

	if(party == ALICE) {
		bool sum = 0, tmp;
		PRG prg;
		prg.random_bool(witness, 2*sz);
		prg.random_bool(coeff+1, sz);
		for(int i = 0; i < sz; ++i) {
			tmp = witness[i] & witness[sz+i];
			sum = sum ^ (coeff[i+1] & tmp);
		}
		coeff[0] = sum;
		ios[0]->send_data(coeff, (sz+1)*sizeof(bool));
	} else {
		ios[0]->recv_data(coeff, (sz+1)*sizeof(bool));
	}
	ios[0]->flush();

	for(int i = 0; i < 2*sz; ++i)
		x[i] = Bit(witness[i], ALICE);

	sync_zk_bool<NetIO>();
	auto start = clock_start();
	for(int j = 0; j < repeat; ++j) {
		zkp_poly_deg2<NetIO>(x, x+sz, coeff, sz);
	}

	bool cheated = finalize_zk_bool<NetIO>();
	if(cheated) error("cheated\n");

	double tt = time_from(start);
	cout << "prove " << repeat << " degree-2 polynomial of length " << sz << endl;
	cout << "time use: " << tt/1000 << " ms" << endl;
	cout << "average time use: " << tt/1000/repeat << " ms" << endl;

	delete[] coeff;
	delete[] witness;
	delete[] x;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ ";
	std::cout << "ZKP polynomial test";
        std::cout << " ------------" << std::endl << std::endl;;
	
	test_polynomial(ios, party);

	for(int i = 0; i < threads; ++i)
		delete ios[i];
	return 0;
}

