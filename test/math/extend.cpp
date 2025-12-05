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
			  << "------------ ZKExtend test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	IntFp *k = new IntFp[dim];
	uint64_t *ori_k = new uint64_t[dim]; 
	for (int i = 0; i < dim; i++){
		uint64_t witness_len = 0;
		ori_k[i] = 0;
		if (party == ALICE){
			witness_len = (i + 2) % (BIT_LENGTH - 1);
			witness[i] = rand() % (1ULL << witness_len);
			witness[i] = rand() % PR;
		
			ori_k[i] = (i + 3) % (BIT_LENGTH - 1);
			if (ori_k[i] + witness_len > (BIT_LENGTH - 1)){
				ori_k[i] = (BIT_LENGTH - 1) - witness_len;
			}
		}
		x[i] = IntFp(witness[i], ALICE);
		k[i] = IntFp(ori_k[i], ALICE);
	}
	ZKExtend(party, x, k, y, dim);
	/****************************/
	/**** verify correctness ****/
	/****************************/
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t ori_y = witness[i] * (1ULL << ori_k[i]);
			uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
			if (ori_y != nm_y){
				cout << "fault !!!" << endl;
			}
		}
	}

	endComputation(party);

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
