#include "emp-vole/emp-vole.h"
#include "emp-tool/emp-tool.h"

using namespace emp;
using namespace std;

int party, port;
const int threads = 5;

void check_triple(NetIO *io, __uint128_t x, __uint128_t* y, int size) {
	if(party == ALICE) {
		io->send_data(&x, sizeof(__uint128_t));
		io->send_data(y, size*sizeof(__uint128_t));
	} else {
		__uint128_t delta;
		__uint128_t *k = new __uint128_t[size];
		io->recv_data(&delta, sizeof(__uint128_t));
		io->recv_data(k, size*sizeof(__uint128_t));
		for(int i = 0; i < size; ++i) {
			__uint128_t tmp = mod(delta*(y[i]>>64), pr);
			tmp = mod(tmp+k[i], pr);
			if(tmp != (y[i]&0xFFFFFFFFFFFFFFFFLL)) {
				std::cout << "triple error at index: " << i << std::endl;
				abort();
			}
		}
	}
	std::cout << "right triple vector" << std::endl;
}

void test_mpfss(NetIO *ios[threads+1], int party) {
	NetIO *io = ios[0];
	ThreadPool pool(threads);
	MpfssRegFp<threads> mpfss(party, N_REG_Fp, T_REG_Fp, BIN_SZ_REG_Fp, &pool, ios);
	mpfss.set_malicious();

	// Preprocessing OT
	OTPre<NetIO> pre_ot(io, mpfss.tree_height-1, mpfss.tree_n);
	BaseCot cot(party, io);
	int pre_ot_n = pre_ot.n;
	cot.cot_gen_pre();
	cot.cot_gen(&pre_ot, pre_ot_n);
	
	// Main protocol
	__uint128_t Delta;
	PRG prg;
	prg.random_data(&Delta, sizeof(__uint128_t));
	Delta = (__uint128_t)((block)Delta & makeBlock((uint64_t)0x0LL, (uint64_t)0xFFFFFFFFFFFFFFFFLL));
	Delta = mod(Delta, pr);
	if(party == ALICE) std::cout << "delta: " << (__uint64_t)Delta << std::endl;
	__uint128_t *ggm_tree = new __uint128_t[N_REG_Fp];
	memset(ggm_tree, 0, N_REG_Fp*sizeof(__uint128_t));

	if(party == ALICE) {
		Base_svole svole(party, io, Delta);
		__uint128_t *y = new __uint128_t[mpfss.tree_n+1];
		svole.triple_gen_send(y, mpfss.tree_n);

		mpfss.sender_init(Delta);
		mpfss.mpfss(&pre_ot, y, ggm_tree);

		check_triple(io, Delta, ggm_tree, N_REG_Fp);
		
		delete[] y;
	} else {
		Base_svole svole(party, io);
		__uint128_t *z = new __uint128_t[mpfss.tree_n+1];
		svole.triple_gen_recv(z, mpfss.tree_n);

		mpfss.recver_init();
		mpfss.mpfss(&pre_ot, z, ggm_tree);

		check_triple(io, 0, ggm_tree, N_REG_Fp);

		delete[] z;
	}

	std::cout << "pass check" << std::endl;
	delete[] ggm_tree;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads+1];
	for(int i = 0; i < threads+1; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ MPFSS ------------" << std::endl << std::endl;;

	test_mpfss(ios, party);

	for(int i = 0; i < threads+1; ++i)
		delete ios[i];
	return 0;
}
