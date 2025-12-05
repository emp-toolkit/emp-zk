#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int rows = 100000;
int cols = 4;

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
			  << "------------ ZKMax test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[rows * cols];
	memset(witness, 0, rows * cols * sizeof(uint64_t));

	startComputation(party);
	
    IntFp *x = new IntFp[rows * cols];
	IntFp *y = new IntFp[rows];
	__uint128_t *randomness = new __uint128_t[rows * cols]; 
	PRG prg(fix_key);
	prg.random_block((block*)randomness, rows * cols);
    for (int i = 0; i < rows * cols; i++){
		if (party == ALICE){
			// |x-y|<=(p-1)/2
			witness[i] = randomness[i] % ((PR+1)/4);
			uint64_t b = randomness[i] & 1;
			if (b==1){
				witness[i] = PR - witness[i]; 
			}
		}
		x[i] = IntFp(witness[i], ALICE);
	}

	uint64_t com = comm(ios);
	auto start = clock_start();

	ZKMax(party, x, y, rows, cols); 

	endComputation(party);

	double time = time_from(start);
	cout << "time - ZKMax: " << time / 1000000 << " s\t " << party << endl;
	uint64_t com1 = comm(ios) - com;
	std::cout << "communication - ZKMax (KB): " << com1 / 1024.0 << std::endl;

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
