#ifndef _LPN_FP_H__
#define _LPN_FP_H__

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-vole/utility.h"
namespace emp {
template<int d = 10>
class LpnFp { public:
	int party;
	int k, n;
	ThreadPool * pool;
	int threads;
	block seed;

	int round, leftover;

	__uint128_t *M;
	const __uint128_t *preM, *prex;
	__uint128_t *K;
	const __uint128_t *preK;

	uint32_t k_mask;

	LpnFp (int n, int k, ThreadPool * pool, int threads, block seed = zero_block) {
		this->k = k;
		this->n = n;
		this->pool = pool;
		this->threads = threads;
		this->seed = seed;
		
		round = d / 4;
		leftover = d % 4;

		k_mask = 1;
		while(k_mask < k) {
			k_mask <<=1;
			k_mask = k_mask | 0x1;
		}
	}

	void add2(int idx1, int* idx2) {
		block Midx1 = (block)M[idx1];
		for(int j = 0; j < 5; ++j) {
			Midx1 = _mm_add_epi64(Midx1, (block) preM[idx2[j]]);
		}
		Midx1 = vec_mod(Midx1);
		for(int j = 5; j < 10; ++j) {
			Midx1 = _mm_add_epi64(Midx1, (block) preM[idx2[j]]);
		}
		Midx1 = vec_mod(Midx1);

		M[idx1] = (__uint128_t)(Midx1);
	}

	void add1(int idx1, int* idx2) {
		uint64_t Kidx1 = 0;
		for(int j = 0; j < 5; ++j) 
			Kidx1 = Kidx1 + preK[idx2[j]];
		Kidx1 = mod(Kidx1);
		for(int j = 5; j < 10; ++j)
			Kidx1 = Kidx1 + preK[idx2[j]];
		K[idx1] +=  mod(Kidx1);
	}

	void __compute4(int i, PRP *prp, std::function<void(int, int*)> add_func) {
		block tmp[10];
		for(int m = 0; m < 10; ++m)
			tmp[m] = makeBlock(i, m);
		prp->permute_block(tmp, 10);
		uint32_t* r = (uint32_t*)(tmp);
		for(int m = 0; m < 4; ++m) {
			int index[d];
			for (int j = 0; j < d; ++j) {
				index[j] = r[m*d+j]&k_mask;
				index[j] = index[j] >= k? index[j]-k:index[j];
			}
			add_func(i+m, index);
		}
	}

	void __compute1(int i, PRP *prp, std::function<void(int, int*)> add_func) {
		block tmp[3];
		for(int m = 0; m < 3; ++m)
			tmp[m] = makeBlock(i, m);
		prp->permute_block(tmp, 3);
		uint32_t* r = (uint32_t*)(tmp);

		int index[d];
		for (int j = 0; j < d; ++j) {
			index[j] = r[j]&k_mask;
			index[j] = index[j] >= k? index[j]-k:index[j];
		}
		add_func(i, index);
	}

	void task(int start, int end) {
		PRP prp(seed);
		int j = start;
		if(party == 1) {
			std::function<void(int, int*)> add_func1 = std::bind(&LpnFp::add1, this, std::placeholders::_1, std::placeholders::_2);
			for(; j < end-4; j+=4)
				__compute4(j, &prp, add_func1);
			for(; j < end; ++j)
				__compute1(j, &prp, add_func1);
		} else {
			std::function<void(int, int*)> add_func2 = std::bind(&LpnFp::add2, this, std::placeholders::_1, std::placeholders::_2);
			for(; j < end-4; j+=4)
				__compute4(j, &prp, add_func2);
			for(; j < end; ++j)
				__compute1(j, &prp, add_func2);
		}
	}

	void compute() {
		vector<std::future<void>> fut;
		int width = n/(threads+1);
		for(int i = 0; i < threads; ++i) {
			int start = i * width;
			int end = min((i+1)* width, n);
			fut.push_back(pool->enqueue([this, start, end]() {
				task(start, end);
			}));
		}
		int start = threads * width;
		int end = min( (threads+1) * width, n);
		task(start, end);

		for (auto &f: fut) f.get();
	}

	void compute_send(__uint128_t *K, const __uint128_t *kkK) {
		this->party = ALICE;
		this->K = K;
		this->preK = kkK;
		compute();
	}

	void compute_recv(__uint128_t *M, const __uint128_t *kkM) {
		this->party = BOB;
		this->M = M;
		this->preM = kkM;
		compute();
	}
};
}
#endif
