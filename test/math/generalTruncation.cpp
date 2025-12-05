#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int dim = 100000;

uint64_t comm(BoolIO<NetIO> *ios[threads])
{
	uint64_t c = 0;
	for (int i = 0; i < threads; ++i)
		c += ios[i]->counter;
	return c;
}

int main(int argc, char **argv)
{
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO> *ios[threads];
	for (int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i), party == ALICE);

	std::cout << std::endl
			  << "------------ ZKgeneralTruncation test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

    IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	uint64_t trunclen = SCALE;

	__uint128_t *randomness = new __uint128_t[dim]; 
	PRG prg(fix_key);
	prg.random_block((block*)randomness, dim);
    for (int i = 0; i < dim; i++){
		if (party == ALICE){
			witness[i] = (uint64_t)randomness[i] % PR;
		}
		x[i] = IntFp(witness[i], ALICE);
	}

	uint64_t com = comm(ios);
	auto start = clock_start();

	ZKgeneralTruncAny(party, x, y, dim, trunclen);

	endComputation(party);

	double time = time_from(start);
	cout << "time - ZKgeneralTruncation (ms): " << time / 1000 << " ms\t " << party << endl;
	uint64_t com1 = comm(ios) - com;
	std::cout << "communication - ZKgeneralTruncation (KB): " << com1 / 1024.0 << std::endl;
	
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		int64_t witness_int = 0;
		uint64_t output_real = 0;
		uint64_t output_prot = 0;
		for (int i = 0; i < dim; i++){
			witness_int = witness[i] > (PR-1)/2 ? witness[i] - PR : witness[i];
			// if (witness[i] > (PR-1)/2 && witness_int < 0){
			// 	cout << "this is a negative value" << endl;
			// }
			witness_int = witness_int >> trunclen;
			output_real = witness_int < 0 ? PR + witness_int : witness_int;
			output_prot = (uint64_t)HIGH64(y[i].value);
			if (output_real != output_prot){
				cout << "fault !!!" << endl;
			}
		}
	}

	cout << "finish test" << endl;

	finalize_zk_bool<BoolIO<NetIO>>();
	finalize_zk_arith<BoolIO<NetIO>>();

	for (int i = 0; i < threads; i++)
	{
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
