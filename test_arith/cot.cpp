#include "emp-vole/emp-vole.h"
#include "emp-tool/emp-tool.h"

using namespace emp;
using namespace std;

int party, port;

void test_cot(NetIO *io, int party) {
	BaseCot cot(party, io);

	int test_n = 1024*128;

	// test single
	auto start = clock_start();
	cot.cot_gen_pre();
	OTPre<NetIO> pre_ot(io, 16, test_n/16);
	if(party == ALICE) {
		cot.cot_gen(&pre_ot, test_n);
	} else {
		cot.cot_gen(&pre_ot, test_n);

		std::cout << "cot generation: " << time_from(start) << " ms" << std::endl;
	}
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO *io = new NetIO(party == ALICE?nullptr:"127.0.0.1",port);

	std::cout << std::endl << "------------ COPE ------------" << std::endl << std::endl;;

	test_cot(io, party);

	delete io;
	return 0;
}
