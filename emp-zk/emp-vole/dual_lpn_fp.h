#ifndef _DUAL_LPN_FP_H__
#define _DUAL_LPN_FP_H__

#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-vole/utility.h"

class DualLpnFp { public:
	int party;
	int k, n;
	ThreadPool * pool;
	int threads;
	block seed;

	__uint128_t *in_vec = nullptr, *out_vec = nullptr;

	const int batch_n = 8;
	const int batch_n_exp = 1<<batch_n;

	DualLpnFp (int n, int k, ThreadPool * pool, int threads, block seed = zero_block) {
		this->k = k;
		this->n = n;
		this->pool = pool;
		this->threads = threads-1;
		this->seed = seed;
	}

	void task(int start, int end, __uint128_t *in_vec_start, int in_length) {
		PRP prp(seed);
		int num = end - start;

		__uint128_t pre_table[batch_n_exp];
		block idx_data[8];
		__uint128_t *data_p = in_vec_start;
		for(int i = 0; i < in_length/batch_n; ++i) {
			pre_table[0] = (__uint128_t)0;
			pre_table[1] = data_p[0];
			for(int k = 1; k < batch_n; ++k) {
				int half_nodes_n = 1 << k;
				for(int j = 0; j < half_nodes_n; ++j) {
					pre_table[half_nodes_n+j] = (__uint128_t)_mm_add_epi64((block)pre_table[j], (block)data_p[k]);
					pre_table[half_nodes_n+j] = (__uint128_t)vec_partial_mod((block)pre_table[j]);
				}
			}

			__uint128_t *out_ptr = out_vec+start;
			for(int j = 0; j < num/16/8; ++j) {
				idx_data[0] = makeBlock(0, i);
				idx_data[1] = makeBlock(1, i);
				idx_data[2] = makeBlock(2, i);
				idx_data[3] = makeBlock(3, i);
				idx_data[4] = makeBlock(4, i);
				idx_data[5] = makeBlock(5, i);
				idx_data[6] = makeBlock(6, i);
				idx_data[7] = makeBlock(7, i);
				prp.permute_block(idx_data, 8);
				uint8_t *idx_ptr = (uint8_t*)idx_data;
				for(int k = 0; k < 16*8; ++k) {
					*out_ptr = (__uint128_t)_mm_add_epi64((block)(*out_ptr), (block)pre_table[idx_ptr[k]]);
					*out_ptr = (__uint128_t)vec_partial_mod((block)(*out_ptr));
					out_ptr++;
				}
			}
			// TODO
			/*if((i%2) == 0) {
				out_ptr = out_vec+start;
				for(int j = 0; j < num; ++j) {
					*out_ptr = (__uint128_t)vec_mod((block)(*out_ptr));
					out_ptr++;
				}
			}*/

			data_p += batch_n;
		}
	}

	void compute(__uint128_t *out, __uint128_t *in) {
		this->in_vec = in;
		this->out_vec = out;
		vector<std::future<void>> fut;
		int width = n/(threads+1);
		for(int i = 0; i < threads; ++i) {
			int start = i * width;
			int end = min((i+1)* width, n);
			fut.push_back(pool->enqueue([this, start, end, in]() {
				task(start, end, in, k);
			}));
		}
		int start = threads * width;
		int end = min( (threads+1) * width, n);
		task(start, end, in, k);

		for (auto &f: fut) f.get();
	}

	// H := (I | U)
	void compute_opt(__uint128_t *out, __uint128_t *in) {
		this->in_vec = in;
		this->out_vec = out;
		vector<std::future<void>> fut;
		memcpy(out, in, n*sizeof(__uint64_t));
		int width = n/(threads+1);
		for(int i = 0; i < threads; ++i) {
			int start = i * width;
			int end = min((i+1)* width, n);
			fut.push_back(pool->enqueue([this, start, end, in]() {
				task(start, end, in+n, k-n);
			}));
		}
		int start = threads * width;
		int end = min( (threads+1) * width, n);
		task(start, end, in+n, k-n);

		for (auto &f: fut) f.get();
	}

};
#endif
