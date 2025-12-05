#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int test_n = 1000;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

void test_LUT(BoolIO<NetIO> *ios[threads], int party)
{

	uint64_t com1 = comm(ios);
	auto start = clock_start();

	vector<uint64_t> data;
	for(int i = 0; i < test_n; ++i)
		data.push_back(2*i);  

	LUTIntFp *LUTram = new LUTIntFp(party);

	LUTram->LUTinit(data);

	for(int i = 0; i < test_n; ++i) {
		IntFp index = IntFp(i, ALICE);
		IntFp value = IntFp(2*i, ALICE);
		LUTram->LUTread(index, value);   

	}

	delete LUTram;

	double time2 = time_from(start);
	std::cout << "time: " << time2 / 1000 << " ms\t " << party << endl;
	uint64_t com2 = comm(ios) - com1;
	std::cout << "communication (KB): " << com2 / 1024.0 << std::endl;
}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ ZKLUT test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	test_LUT(ios, party);

	cout << "finish test" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < threads; ++i)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
