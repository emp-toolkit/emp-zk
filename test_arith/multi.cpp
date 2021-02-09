#include <iostream>
#include "vole-arith/utility.h"
using namespace std;

int main(){
	vector<block> b;
	b.resize(10000);
	PRG prg;
	prg.random_block(b.data(), 10000);
	uint64_t a = 1231245112312;
	uint64_t aa[4];
	for(auto &v : aa)
		v = a;
	auto t1 = clock_start();
	for(int j = 0; j < 10000; ++j)
		for(int i = 0; i < 10000; ++i)
			b[i] = mult_mod(b[i], a);
	cout << time_from(t1)/10000.0/10000.0*1000<<endl;

	t1 = clock_start();
	for(int j = 0; j < 10000; ++j)
		for(int i = 0; i < 10000; i+=2)
			mult_mod_bch2(b.data()+i, b.data()+i, aa);
	cout << time_from(t1)/10000.0/10000.0*1000<<endl;

	for(int i = 0; i < 10000; ++i)
		cout << b[i]<<endl;
}