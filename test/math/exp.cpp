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
			  << "------------ ZKExp test ------------" << std::endl
			  << std::endl;

	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);

	sync_zk_bool<BoolIO<NetIO>>();

	uint64_t *witness = new uint64_t[dim];
	memset(witness, 0, dim * sizeof(uint64_t));

	startComputation(party);

	uint64_t constant = (1ULL << 16);
	IntFp *x = new IntFp[dim];
	IntFp *y = new IntFp[dim];
	__uint128_t *randomness = new __uint128_t[dim]; 
	PRG prg(fix_key);
	prg.random_block((block*)randomness, dim);
    for (int i = 0; i < dim; i++){
		if (party == ALICE){
			witness[i] = randomness[i] % constant;
		}
		x[i] = IntFp(witness[i], ALICE);
	}

	uint64_t com = comm(ios);
	auto start = clock_start();

	ZKExp(party, x, y, dim);
	endComputation(party);

	double time = time_from(start);
	cout << "time - ZKExp: " << time / 1000000 << " s\t " << party << endl;
	uint64_t com1 = comm(ios) - com;
	std::cout << "communication - ZKExp (KB): " << com1 / 1024.0 << std::endl;

	/****************************/
	/**** verify correctness ****/
	/****************************/
	uint64_t total_err_fixed = 0;
    uint64_t max_ULP_err_fixed = 0;
	if (party == ALICE){
		uint64_t last_digit_bitlen = EXP_N - (EXP_DIGIT_LEN) * (EXP_LUT_NUM - 1);
		for (int i = 0; i < dim; i++){
			// exp on each sub-string
			uint64_t *value_field = new uint64_t[EXP_LUT_NUM];
			for (int j = 0; j < EXP_LUT_NUM - 1; j++){
				uint64_t digit = (witness[i] >> (j * EXP_DIGIT_LEN)) & ((1ULL << EXP_DIGIT_LEN) - 1);
				double digit_real = double(digit) / double(1ULL << SCALE);
            	double value_real = exp(-1 * (digit_real * (1ULL << EXP_DIGIT_LEN * j)));
				value_field[j] = value_real * (1ULL << SCALE);
			}
			uint64_t digit = (witness[i] >> ((EXP_LUT_NUM - 1) * EXP_DIGIT_LEN)) & ((1ULL << last_digit_bitlen) - 1);
			double digit_real = double(digit) / double(1ULL << SCALE);
            double value_real = exp(-1 * (digit_real * (1ULL << EXP_DIGIT_LEN * (EXP_LUT_NUM - 1))));
			value_field[EXP_LUT_NUM - 1] = value_real * (1ULL << SCALE);
			// multiply
			uint64_t ori_y = value_field[0];
			for (int j = 1; j < EXP_LUT_NUM; j++){
				ori_y = ori_y * value_field[j] >> SCALE;
			}
			uint64_t nm_y = (uint64_t)HIGH64(y[i].value);
			if (ori_y != nm_y){
				cout << "exp fault !!!" << "real: " << ori_y << ", protocol: " << nm_y << endl;
			}

			/****************************/
			/****** verify ULPerror *****/
			/****************************/
			double witness_real = Field2Real(witness[i], SCALE);
			double exp = std::exp(-1 * witness_real);
			uint64_t exp_field = Real2Field(exp, SCALE);
			uint64_t err_fixed = computeULPErr(ori_y, exp_field);
      		// if (err_fixed > 1)
      		// {
        	// 	cout << "ULP Error Fixed: " << ori_y << "," << exp_field << ","
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
