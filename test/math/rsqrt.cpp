#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include <iostream>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;

int dim = 1000;

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
			  << "------------ ZKrSqrt test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	int iter = 1;
	uint64_t constant = (1ULL << SQRT_N);
	__uint128_t *randomness = new __uint128_t[dim]; 
	PRG prg(fix_key);
	prg.random_block((block*)randomness, dim);
    for (int i = 0; i < dim; i++){
		if (party == ALICE){
			witness[i] = randomness[i] % constant;
			if (witness[i] <= 1){
				witness[i] = witness[i] + 1;
			}
		}
		x[i] = IntFp(witness[i], ALICE);
	}

	uint64_t com = comm(ios);
	auto start = clock_start();

	ZKrSqrt(party, x, y, dim, iter);
	
	endComputation(party);

	double time = time_from(start);
	cout << "time - ZKrSqrt: " << time / 1000000 << " s\t " << party << endl;
	uint64_t com1 = comm(ios) - com;
	std::cout << "communication - ZKrSqrt (KB): " << com1 / 1024.0 << std::endl;

	/****************************/
	/**** verify correctness ****/
	/****************************/
	uint64_t total_err_fixed = 0;
    uint64_t max_ULP_err_fixed = 0;
	if (party == ALICE){
		for (int i = 0; i < dim; i++){
			uint64_t k = floor(log2(int64_t(witness[i])));
			uint64_t z = witness[i] * (1ULL << (SQRT_N - 1- k));
			uint64_t k0 = k & 1ULL;  
			uint64_t z1 = (z >> (SQRT_N - 1 - SQRT_M)) & ((1ULL << SQRT_M) - 1);
			// Table Lookup
			double z1bar = 1 + (double(z1) / double(1ULL << SQRT_M));
			double t_real = 1/sqrt((double)((k0 + 1) * z1bar)); 
			uint64_t t = t_real * (1ULL << SCALE);
			// iteration
			uint64_t a = ((k0 + 1) * z) >> (SQRT_N - 1- SCALE);
			uint64_t b = t;
			uint64_t c = b;
			for (int j = 0; j < iter; j++){
				a = (b * b * a) >> (2 * SCALE);
				b = 3 * (1ULL << SCALE) - a;
				c = (c * b) >> (SCALE + 1);
			}
			double tmp = floor((double)(SCALE + 1 - (int)k)/2.0) + (double)(SQRT_N - SCALE)/2.0;
			uint64_t extendlen = tmp;
			uint64_t ori_y = (c * (1ULL << extendlen)) >> ((SQRT_N - SCALE)/2);
			uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
			if (ori_y != nm_y){
				cout << "rSqrt fault !!!  " << "k: " << k << ", tmp: " << tmp << ", real: " << ori_y << ", protocol: " << nm_y << endl;
			}

			/****************************/
			/****** verify ULPerror *****/
			/****************************/
			double witness_real = Field2Real(witness[i], SCALE);
			double div = 1.0/sqrt(witness_real);
			uint64_t div_field = Real2Field(div, SCALE);
			uint64_t err_fixed = computeULPErr(ori_y, div_field);
      		// if (err_fixed > 1)
      		// {
        	// 	cout << "ZKsqrt ULP Error Fixed: " << ori_y << "," << div_field << ","
            //  		<< err_fixed << endl;
      		// }
      		total_err_fixed += err_fixed;
      		max_ULP_err_fixed = std::max(max_ULP_err_fixed, err_fixed);
		}
		cout << "Average ULP error fixed: " << total_err_fixed / dim << endl;
    	cout << "Total ULP error fixed: " << total_err_fixed << endl;
    	cout << "Max ULP error fixed: " << max_ULP_err_fixed << endl;
    	cout << "Number of tests fixed: " << dim << endl;
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
