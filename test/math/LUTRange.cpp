#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int test_n = 5;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

void test_LUTRange(BoolIO<NetIO> *ios[threads], int party)
{
	uint64_t com1 = comm(ios);
	auto start = clock_start();

	LUTRangeIntFp *LUTRange = new LUTRangeIntFp(party);

	uint64_t lutSize = 100;
	LUTRange->LUTRangeinit(lutSize);

	IntFp index = IntFp((uint64_t)20, ALICE);
	LUTRange->LUTRangeread(index); 

	delete LUTRange;

	double time2 = time_from(start);
	std::cout << "time: " << time2 / 1000 << " ms\t " << party << endl;
	uint64_t com2 = comm(ios) - com1;
	std::cout << "communication (KB): " << com2 / 1024.0 << std::endl;
}

void test_range_single(BoolIO<NetIO> *ios[threads], int party)
{
	uint64_t com1 = comm(ios);
	auto start = clock_start();

	LUTRangeIntFp *LUTSimpleRange = new LUTRangeIntFp(party);

	uint64_t lutSize = 100;

	LUTSimpleRange->LUTRangeinit(lutSize);

	IntFp index = IntFp((uint64_t)20, ALICE);
	LUTSimpleRange->LUTRangeSimplecheck(index);

	bool res = batch_reveal_check_zero(LUTSimpleRange->batch_simplecheck.data(), LUTSimpleRange->batch_simplecheck.size());

	cout << "res = " << res << endl;

	delete LUTSimpleRange;

	double time2 = time_from(start);
	std::cout << "time - Simple Check: " << time2 / 1000 << " ms\t " << party << endl;
	uint64_t com2 = comm(ios) - com1;
	std::cout << "communication (KB) - Simple Check: " << com2 / 1024.0 << std::endl;
}


int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ ZKLUTRange test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	test_LUTRange(ios, party);
	cout << "-------------- finish test 1 ---------------" << endl;
	test_range_single(ios, party);
	cout << "-------------- finish test 2 ---------------" << endl;

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
