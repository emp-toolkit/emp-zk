#include "vole-arith/emp-vole.h"
#include "emp-tool/emp-tool.h"

using namespace emp;
using namespace std;

int party, port;

void test_spfss(NetIO *io, int party) {
	int tree_height = 18;
	int leave_n = 1<<(tree_height-1);
	PRG prg;
	
	int pre_ot_n = tree_height - 1;
	OTPre<NetIO> pre_ot(io, pre_ot_n, 1);
	BaseCot cot(party, io);
	cot.cot_gen_pre();
	cot.cot_gen(&pre_ot, pre_ot_n);
	
	__uint128_t Delta;
	prg.random_data(&Delta, sizeof(__uint128_t));
	Delta = (__uint128_t)((block)Delta & makeBlock((uint64_t)0x0LL, (uint64_t)0xFFFFFFFFFFFFFFFFLL));
	Delta = mod(Delta, pr);
	__uint128_t *ggm_tree = new __uint128_t[leave_n];
	memset(ggm_tree, 0, leave_n*sizeof(__uint128_t));


	if(party == ALICE) {
		SpfssSenderFp<NetIO> *sender = new SpfssSenderFp<NetIO>(io, tree_height);
		Base_svole svole(party, io, Delta);
		__uint128_t triple[1024];
		svole.triple_gen_send(triple, 1024);
		io->flush();

		pre_ot.choices_sender();
		std::cout << "delta: " << (block)Delta << std::endl;
		sender->compute(ggm_tree, Delta, triple[0]);
		sender->send<OTPre<NetIO>>(&pre_ot, io, 0);
		io->flush();

		sender->consistency_check(io, triple[1]);
	} else {
		SpfssRecverFp<NetIO> *recver = new SpfssRecverFp<NetIO>(io, tree_height);
		Base_svole svole(party, io);
		__uint128_t triple[1024];
		svole.triple_gen_recv(triple, 1024);

		srand(time(0));
		recver->choice_bit_gen(rand()%leave_n);
		
		pre_ot.choices_recver(recver->b);
		recver->recv<OTPre<NetIO>>(&pre_ot, io, 0);
		recver->compute(ggm_tree, triple[0]);

		recver->consistency_check(io, triple[1], triple[0]);
	}

	delete[] ggm_tree;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* io;
	io = new NetIO(party == ALICE?nullptr:"127.0.0.1",port);

	std::cout << std::endl << "------------ SPFSS ------------" << std::endl << std::endl;;

	test_spfss(io, party);

	delete io;
	return 0;
}
