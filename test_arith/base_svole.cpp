#include "emp-vole/emp-vole.h"
#include "emp-tool/emp-tool.h"

using namespace emp;
using namespace std;

int party, port;

void test_base_svole(NetIO *io, int party) {
	int test_n = 1024*1024;
	__uint128_t *mac = new __uint128_t[test_n];

	Base_svole *svole;

	__uint128_t Delta;
	if(party == ALICE) {
		PRG prg;
		prg.random_data(&Delta, sizeof(__uint128_t));
		Delta = Delta & ((__uint128_t)0xFFFFFFFFFFFFFFFFLL);
		Delta = mod(Delta, pr);

		svole = new Base_svole(party, io, Delta);
	} else {
		svole = new Base_svole(party, io);
	}

	// test single
	auto start = clock_start();
	if(party == ALICE) {
		svole->triple_gen_send(mac, test_n);

		//svole->cope->check_triple(&Delta, mac, test_n);
	} else {
		svole->triple_gen_recv(mac, test_n);
		std::cout << "base svole: " << time_from(start) << " ms" << std::endl;
		//svole->cope->check_triple(nullptr, mac, test_n);
	}

	start = clock_start();
	if(party == ALICE) {
		svole->triple_gen_send(mac, test_n);

		//svole->cope->check_triple(&Delta, mac, test_n);
	} else {
		svole->triple_gen_recv(mac, test_n);
		std::cout << "base svole: " << time_from(start) << " ms" << std::endl;
		//svole->cope->check_triple(nullptr, mac, test_n);
	}
	std::cout << "pass check" << std::endl;

	delete[] mac;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO *io = new NetIO(party == ALICE?nullptr:"127.0.0.1",port);

	std::cout << std::endl << "------------ COPE ------------" << std::endl << std::endl;;

	test_base_svole(io, party);

	delete io;
	return 0;
}
