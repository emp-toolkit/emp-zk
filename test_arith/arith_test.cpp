#include "vole-arith/emp-vole.h"
#include "emp-tool/emp-tool.h"

using namespace emp;
using namespace std;

void test_mult_mod_bch2_mac() {
	block aa[2];
	block bb;
	PRG prg;
	prg.random_block(aa, 2);
	prg.random_block(&bb, 1);
	uint64_t *a = (uint64_t*)aa;
	uint64_t *b = (uint64_t*)&bb;
	for(int i = 0; i < 4; i++)
		a[i] = mod(a[i]);
	for(int i = 0; i < 2; ++i)
		b[i] = mod(b[i]);

	block res[2];
	mult_mod_bch2(res, aa, b);
	uint64_t *ress = (uint64_t*)res;
	for(int i = 0; i < 4; ++i) {
		if(ress[i] != mult_mod(a[i], b[i/2]))
			std::cout << "wrong mult mac 2" << std::endl;
	}
}

void test_mult_mod_bch2_key() {
	block aa;
	block bb;
	PRG prg;
	prg.random_block(&aa, 1);
	prg.random_block(&bb, 1);
	uint64_t *a = (uint64_t*)&aa;
	uint64_t *b = (uint64_t*)&bb;
	for(int i = 0; i < 2; ++i) {
		a[i] = mod(a[i]);
		b[i] = mod(b[i]);
	}
	uint64_t res[2];
	mult_mod_bch2(res, a, b);
	for(int i = 0; i < 2; ++i) {
		if(res[i] != mult_mod(a[i], b[i]))
			std::cout << "wrong mult key 2" << std::endl;
	}
}
#ifdef __AVX512F__
void test_mult_mod_bch4_mac() {
	block aa[4];
	block bb[2];
	PRG prg;
	prg.random_block(aa, 4);
	prg.random_block(bb, 2);
	uint64_t *a = (uint64_t*)aa;
	uint64_t *b = (uint64_t*)bb;
	for(int i = 0; i < 8; i++)
		a[i] = mod(a[i]);
	for(int i = 0; i < 4; ++i)
		b[i] = mod(b[i]);

	block res[4];
	mult_mod_bch4(res, aa, b);
	uint64_t *ress = (uint64_t*)res;
	for(int i = 0; i < 8; ++i) {
		if(ress[i] != mult_mod(a[i], b[i/2]))
			std::cout << "wrong mult mac" << std::endl;
	}
}

void test_mult_mod_bch4_key() {
	block aa[2];
	block bb[2];
	PRG prg;
	prg.random_block(aa, 2);
	prg.random_block(bb, 2);
	uint64_t *a = (uint64_t*)aa;
	uint64_t *b = (uint64_t*)bb;
	for(int i = 0; i < 4; ++i) {
		a[i] = mod(a[i]);
		b[i] = mod(b[i]);
	}
	uint64_t res[4];
	mult_mod_bch4(res, a, b);
	for(int i = 0; i < 4; ++i) {
		if(res[i] != mult_mod(a[i], b[i]))
			std::cout << "wrong mult key" << std::endl;
	}
}
#endif
int main(int argc, char** argv) {
#ifdef __AVX512F__
	test_mult_mod_bch4_mac();
	test_mult_mod_bch4_key();
#endif
	return 0;
}
