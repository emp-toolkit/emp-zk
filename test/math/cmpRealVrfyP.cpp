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
			  << "------------ ZKcmpVrfyP test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	uint64_t constant = PR;
    IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	__uint128_t *randomness = new __uint128_t[dim]; 
	PRG prg(fix_key);
	prg.random_block((block*)randomness, dim);
    for (int i = 0; i < dim; i++){
		if (party == ALICE){
			witness[i] = (uint64_t)randomness[i] % constant;
		}
		x[i] = IntFp(witness[i], ALICE);
	}


	uint64_t com = comm(ios);
	auto start = clock_start();

	ZKcmpRealVrfyP(party, x, constant, y, dim);

	endComputation(party);

	double time = time_from(start);
	cout << "time - ZKcmpVrfyP (ms): " << time / 1000 << " ms\t " << party << endl;
	uint64_t com1 = comm(ios) - com;
	std::cout << "communication - ZKcmpVrfyP (KB): " << com1 / 1024.0 << std::endl;

	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_z = 0;
			if (witness[i] < constant){
				ori_z = 1;
			}
			uint64_t nm_z = (uint64_t)HIGH64(y[i].value);
			if (ori_z != nm_z){
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
