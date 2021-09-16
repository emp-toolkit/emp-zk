#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-vole/dual_lpn_fp.h"

int party, port;

int main(int argc, char **argv) {
	parse_party_and_port(argv, &party, &port);
	int threads = 4;
	ThreadPool pool(threads);
	int n = 9012;
	int k = 65536;
	DualLpnFp lpn(n, k, &pool, threads);

	__uint128_t *out = new __uint128_t[n];
	__uint128_t *in = new __uint128_t[k];
	PRG prg;
	prg.random_block((block*)in, k);

	auto start = clock_start();
	lpn.compute(out, in);

	std::cout << time_from(start)/1000.0 << " ms" << std::endl;

	delete[] out;
	delete[] in;

	return 0;
}
