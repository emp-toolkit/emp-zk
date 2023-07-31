#include <iostream>
#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk.h"
#include "emp-zk/extensions/ram-zk/ro-zk-mem-ext.h"
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
//int index_sz = 23, val_sz = 32;
int index_sz = 5, val_sz = 32, step_sz = 10;


void test(BoolIO<NetIO> *ios[threads], int party) {
	setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
	vector<vector<Integer>> data;
	int test_n = 20;
	for(int i = 0; i < test_n; ++i)
		data.push_back(vector<Integer>(step_sz, Integer(val_sz, 2*i, ALICE)));
    ROZkRamExt<BoolIO<NetIO>> *roram = new ROZkRamExt<BoolIO<NetIO>>(party, index_sz, val_sz, step_sz);
    roram->init(data);
	
    for(int i = 0; i < test_n; ++i) {
		vector<Integer> res = roram->read(Integer(index_sz, i, PUBLIC));
        Bit eq(true, PUBLIC); 
        for (int j = 0; j < step_sz; j++) {
			eq = eq & (res[j] == Integer(val_sz, i*2, ALICE));
		}
        if(!eq.reveal<bool>(PUBLIC)) {
			cout << i <<"something is wrong!!\n";
		}
	}
	roram->check();
	delete roram;
	cout << "done check roram" << endl;
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

	test(ios, party);

	for(int i = 0; i < threads; ++i) {
		delete ios[i]->io;
		delete ios[i];
	}
	return 0;
}
