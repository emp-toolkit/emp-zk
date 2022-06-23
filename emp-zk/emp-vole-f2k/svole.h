#ifndef _VOLE_F2K_H_
#define _VOLE_F2K_H_
#include "emp-zk/emp-vole-f2k/base_svole.h"
#include "emp-zk/emp-vole-f2k/lpn_f2k.h"
#include "emp-zk/emp-vole-f2k/mpfss.h"

namespace emp {
template<typename IO>
class SVoleF2k { 
public:
	IO * io;
	IO **ios;
	int party;
	int threads;
	int n, t, k, log_bin_sz;
	int n_pre, t_pre, k_pre, log_bin_sz_pre;
	//int n_pre0, t_pre0, k_pre0, log_bin_sz_pre0;
	int M;
	int ot_used, ot_limit;
	bool is_malicious;
	bool extend_initialized;
	bool pre_ot_inplace;
	block *pre_yz = nullptr;
	block *pre_x = nullptr;
	block *vole_yz = nullptr;
	block *vole_x = nullptr;

	FerretCOT<IO> *ferret = nullptr;
	BaseSVoleF2k<IO> *base_svole = nullptr;
	BaseCot<IO> *base_cot = nullptr; // TODO use ferret
	MpfssRegF2k<IO> *mpfss = nullptr;
	OTPre<IO> *ot_pre = nullptr;

	block Delta;
	LpnF2k<LPN_D> *lpn = nullptr;
	ThreadPool *pool = nullptr;

	SVoleF2k (int party, int threads, IO **ios, FerretCOT<IO> *ferret) {
        	this->io = ios[0];
		this->threads = threads;
		this->party = party;
		this->ios = ios;
		this->ferret = ferret;
		set_param();
		set_preprocessing_param();
		this->extend_initialized = false;
		pool = new ThreadPool(threads);
		base_cot = new BaseCot<IO>(3-party, ios[0], true);
	}

	~SVoleF2k() {
		if(pre_yz != nullptr) delete[] pre_yz;
		if(pre_x != nullptr) delete[] pre_x;
		if(lpn != nullptr) delete lpn;
		if(pool != nullptr) delete pool;
		if(vole_yz != nullptr) delete[] vole_yz;
		if(vole_x != nullptr) delete[] vole_x;
		if(base_svole != nullptr) delete base_svole;
		if(base_cot != nullptr) delete base_cot;
		if(mpfss != nullptr) delete mpfss;
		if(ot_pre != nullptr) delete ot_pre;
	}

	void set_param() {
		this->n = N_REG;
		this->k = K_REG;
		this->t = T_REG;
		this->log_bin_sz = BIN_SZ_REG;
	}

	void set_preprocessing_param() {
		this->n_pre = N_PRE_REG;
		this->k_pre = K_PRE_REG;
		this->t_pre = T_PRE_REG;
		this->log_bin_sz_pre = BIN_SZ_PRE_REG;
		/*this->n_pre0 = N_PRE0_REG;
		this->k_pre0 = K_PRE0_REG;
		this->t_pre0 = T_PRE0_REG;
		this->log_bin_sz_pre0 = BIN_SZ_PRE0_REG;*/
	}

	void setup(block delta) {
		this->Delta = delta;
		setup();
	}

	block delta() {
		if(party == BOB)
			return this->Delta;
		else {
			error("No delta for ALICE");
			return zero_block;
		}
	}

	void extend_initialization() {
		lpn = new LpnF2k<LPN_D>(n, k, pool, pool->size());
		base_svole = new BaseSVoleF2k<IO>(party, ios, ferret);
		ot_pre = new OTPre<IO>(ios[0], log_bin_sz, t);
		mpfss = new MpfssRegF2k<IO>(3-party, threads, n, t, log_bin_sz, pool, ios);
		mpfss->set_malicious();
		M = k + t + 1;
		ot_limit = n - M;
		ot_used = ot_limit;
		extend_initialized = true;
	}

	// sender extend
	void extend_send(block *val, 
			block *mac,
			block *pre_val,
			block *pre_mac,
			int tt,
			MpfssRegF2k<IO> *mpfss,
			OTPre<IO> *ot_pre,
			LpnF2k<LPN_D> *lpn) {
		mpfss->recver_init();
		mpfss->mpfss(ot_pre, pre_val, pre_mac, mac);
		mpfss->set_vec_x(val);
		lpn->compute_send(pre_val+tt, val, pre_mac+tt, mac);
	}

	// receiver extend
	void extend_recv(block *mac,
			block *pre_mac,
			int tt,
			MpfssRegF2k<IO> *mpfss,
			OTPre<IO> *ot_pre,
			LpnF2k<LPN_D> *lpn) {
		mpfss->sender_init(Delta);
		mpfss->mpfss(ot_pre, pre_mac, mac);
		lpn->compute_recv(pre_mac+tt, mac);
	}

	void extend(block *x, block *yz) {
		base_cot->cot_gen(ot_pre, ot_pre->n);
		if(party == ALICE) {
			memset(x, 0, n*sizeof(block));
			extend_send(x, yz, pre_x, pre_yz, t, mpfss, ot_pre, lpn);
			memcpy(pre_x, x+ot_limit, M*sizeof(block));
		} else extend_recv(yz, pre_yz, t, mpfss, ot_pre, lpn);
		memcpy(pre_yz, yz+ot_limit, M*sizeof(block));
	}

	void setup() {
		extend_initialization();
		
		if(party == ALICE) base_cot->cot_gen_pre();
		else base_cot->cot_gen_pre(Delta);

		// space for pre-processing triples
		int M_pre = k_pre + t_pre + 1;
		pre_yz = new block[n_pre];
		block *pre_x0 = nullptr;
		block *pre_yz0 = new block[M_pre];
		memset(pre_yz, 0, n_pre*sizeof(block));
		memset(pre_yz0, 0, M_pre*sizeof(block));
		if(party == ALICE) {
			pre_x = new block[n_pre];
			pre_x0 = new block[M_pre];
			memset(pre_x, 0, n_pre*sizeof(block));
			memset(pre_x0, 0, M_pre*sizeof(block));
		}

		// pre-processing tools
		LpnF2k<LPN_D> lpn_pre(n_pre, k_pre, pool, pool->size());
		BaseSVoleF2k<IO> base_svole_pre(party, ios, ferret);
		OTPre<IO> ot_pre1(ios[0], log_bin_sz_pre, t_pre);
		MpfssRegF2k<IO> mpfss_pre(3-party, threads, n_pre, t_pre, log_bin_sz_pre, pool, ios);
		mpfss_pre.set_malicious();

		// generate tree_n*(depth-1) COTs
		base_cot->cot_gen(&ot_pre1, ot_pre1.n);
		base_svole_pre.extend(pre_x0, pre_yz0, M_pre);

		// generate 2*tree_n+k_pre triples and extend
		if(party == ALICE) {
			extend_send(pre_x, pre_yz, pre_x0, pre_yz0, t_pre, &mpfss_pre, &ot_pre1, &lpn_pre);
		} else {
			extend_recv(pre_yz, pre_yz0, t_pre, &mpfss_pre, &ot_pre1, &lpn_pre);
		}
		pre_ot_inplace = true;

		delete[] pre_yz0;
		if(party == ALICE) delete[] pre_x0;

		vole_yz = new block[n];
		if(party == ALICE) vole_x = new block[n];
	}

	uint64_t extend_inplace(block *data_x, block *data_yz, int byte_space) {
		if(byte_space < n) error("space not enough");
		uint64_t tp_output_n = byte_space - M;
		if(tp_output_n % ot_limit != 0)
			error("call byte_memory_need_inplace \
				to get the correct length of memory space");
		int round = tp_output_n / ot_limit;
		block *pt = data_yz;
		if(party == ALICE) {
			block *ptx = data_x;
			for(int i = 0; i < round; ++i) {
				extend(ptx, pt);
				ptx += ot_limit;
				pt += ot_limit;
			}
		} else {
			block *ptx = nullptr;
			for(int i = 0; i < round; ++i) {
				extend(ptx, pt);
				pt += ot_limit;
			}
		}
		return tp_output_n;
	}

	uint64_t byte_memory_need_inplace(uint64_t tp_need) {
		int round = (tp_need - 1) / ot_limit;
		return round * ot_limit + n;
	}

	int silent_ot_left() {
		return ot_limit - ot_used;
	}
};
}
#endif
