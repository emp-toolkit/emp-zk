#include <iostream>
#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
//int index_sz = 23, val_sz = 32;
int index_sz = 15, val_sz = 32;

void bench(BoolIO<NetIO> *ios[threads], int party) {
	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	vector<Integer> data;
	int test_n = (1<<index_sz)*4;
	for(int i = 0; i < (1<<index_sz); ++i)
		data.push_back(Integer(val_sz, 2*i, PUBLIC));
	sync_zk_bool<BoolIO<NetIO>>();
	auto start=clock_start();
	ROZKRAM<BoolIO<NetIO>> *ram = new ROZKRAM<BoolIO<NetIO>>(party, index_sz, val_sz);
	ram->init(data);
	std::cout <<"init:"<<time_from(start)/1000<<endl;
	Integer ind(index_sz, 0, PUBLIC);
	start=clock_start();
	for(int j = 0; j < test_n/(1<<index_sz); ++j) {
		for(int i = 0; i < (1<<index_sz); ++i)
			Integer res = ram->read(ind);
		ram->check();
	}
	std::cout <<" total (us):"<<time_from(start)/test_n<<endl;
	std::cout << "access (us): " << ram->check1/test_n << std::endl;
	std::cout << "check condition (us): " << ram->check2/test_n << std::endl;
	std::cout << "check set equality (us):" << ram->check3/test_n << std::endl;
	delete ram;
	finalize_zk_bool<BoolIO<NetIO>>();
}


void test(BoolIO<NetIO> *ios[threads], int party) {
	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	vector<Integer> data;
	int test_n = (1<<index_sz);
	for(int i = 0; i < test_n; ++i)
		data.push_back(Integer(val_sz, 2*i, ALICE));
	ROZKRAM<BoolIO<NetIO>> *ram = new ROZKRAM<BoolIO<NetIO>>(party, index_sz, val_sz);
	ram->init(data);
	for(int i = 0; i < test_n; ++i) {
		Integer res = ram->read(Integer(index_sz, i, PUBLIC));
		Bit eq = res == Integer(val_sz, i*2, ALICE);
		if(!eq.reveal<bool>(PUBLIC)) {
			cout <<i<<"something is wrong!!\n";
		}
	}
	ram->check();
	delete ram;
	finalize_zk_bool<BoolIO<NetIO>>();
	cout <<"done\n";
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	BoolIO<NetIO>* ios[threads];
	for(int i = 0; i < threads; ++i)
		ios[i] = new BoolIO<NetIO>(new NetIO(party == ALICE?nullptr:"127.0.0.1",port), party==ALICE);

	if (argc > 3)
		index_sz = atoi(argv[3]);
	else index_sz = 10;

	test(ios, party);
	bench(ios, party);

	for(int i = 0; i < threads; ++i) {
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
