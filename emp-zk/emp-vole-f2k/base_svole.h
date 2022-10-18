#ifndef BASE_SVOLE_F2K_H__
#define BASE_SVOLE_F2K_H__
#include "emp-ot/emp-ot.h"

namespace emp {


template<typename IO>
class BaseSVoleF2k {
public:
	int party;
	IO **ios;
	IO *io;
	FerretCOT<IO> *ferret = nullptr;
	block delta;

	GaloisFieldPacking pack;

	int ferret_cnt, f2k_cnt;
	block *ferret_buffer = nullptr;
	block *f2k_val_buffer = nullptr;
	block *f2k_mac_buffer = nullptr;

	int64_t FERRET_BUFFER_MEM_SZ;
	int64_t FERRET_BUFFER_SZ;
	int64_t F2K_BUFFER_SZ;

	BaseSVoleF2k (int party, IO **ios, FerretCOT<IO> *ferret) :
		party(party), ios(ios), ferret(ferret) {
		FERRET_BUFFER_MEM_SZ = ferret_b13.n;
		FERRET_BUFFER_SZ = ferret_b13.buf_sz();
		F2K_BUFFER_SZ = ferret_b13.buf_sz()/128;

		if(party == BOB) delta = ferret->Delta;
		io = ios[0];

		ferret_cnt = 0;
		f2k_cnt = 0;

		ferret_buffer = new block[FERRET_BUFFER_MEM_SZ];
		if(party == ALICE) {
			f2k_val_buffer = new block[F2K_BUFFER_SZ];
		}
		f2k_mac_buffer = new block[F2K_BUFFER_SZ];

		ferret_buffer_refill();
		pre_f2k_buffer_refill();
	}

	~BaseSVoleF2k() {
		delete[] ferret_buffer;
		if(party == ALICE) delete[] f2k_val_buffer;
		delete[] f2k_mac_buffer;
	}

	void extend(block *val, block *mac, int num) {
		int buf_left = F2K_BUFFER_SZ - f2k_cnt;
		int rd1_n = (buf_left>=num)?num:buf_left;
		int rd_n = 0, rdext_n = 0;
		if(rd1_n < num) { int tmp = num - rd1_n; rd_n = tmp / F2K_BUFFER_SZ; rdext_n = tmp % F2K_BUFFER_SZ; }
		int pt = rd1_n;
		if(party == ALICE) {
			memcpy(val, f2k_val_buffer+f2k_cnt, rd1_n*sizeof(block));
			memcpy(mac, f2k_mac_buffer+f2k_cnt, rd1_n*sizeof(block));
			for(int i = 0; i < rd_n; ++i) {
				pre_f2k_buffer_refill();
				memcpy(val+pt, f2k_val_buffer, F2K_BUFFER_SZ*sizeof(block));
				memcpy(mac+pt, f2k_mac_buffer, F2K_BUFFER_SZ*sizeof(block));
				pt += F2K_BUFFER_SZ;
			}
			pre_f2k_buffer_refill();
			memcpy(val+pt, f2k_val_buffer, rdext_n*sizeof(block));
			memcpy(mac+pt, f2k_mac_buffer, rdext_n*sizeof(block));
		} else {
			memcpy(mac, f2k_mac_buffer+f2k_cnt, rd1_n*sizeof(block));
			for(int i = 0; i < rd_n; ++i) {
				pre_f2k_buffer_refill();
				memcpy(mac+pt, f2k_mac_buffer, F2K_BUFFER_SZ*sizeof(block));
				pt += F2K_BUFFER_SZ;
			}
			pre_f2k_buffer_refill();
			memcpy(mac+pt, f2k_mac_buffer, rdext_n*sizeof(block));
		}
		f2k_cnt = rdext_n;
	}

	void pre_f2k_buffer_refill() {
		int f2k_cnt_tmp = 0;
		while(f2k_cnt_tmp < F2K_BUFFER_SZ) {
			if((ferret_cnt + 128) > FERRET_BUFFER_SZ) {
				ferret_buffer_refill();
			}
			if(party == ALICE) {
				bool val[128];
				for(int i = 0; i < 128; ++i)
					val[i] = getLSB(ferret_buffer[ferret_cnt+i]);
				f2k_val_buffer[f2k_cnt_tmp] = bool_to_block(val);
			}
			pack.packing(f2k_mac_buffer+f2k_cnt_tmp, ferret_buffer+ferret_cnt);
			ferret_cnt += 128;
			f2k_cnt_tmp++;
		}
		f2k_cnt = 0;
	}

	void ferret_buffer_refill() {
		ferret->rcot_inplace(ferret_buffer, FERRET_BUFFER_MEM_SZ);
		ferret_cnt = 0;
	}
	

	// DEBUG
	void check_correctness(block *val, block *mac, int num) {
		if(party == ALICE) {
			io->send_data(val, num*sizeof(block));
			io->send_data(mac, num*sizeof(block));
		} else {
			block *vr = new block[num];
			block *mr = new block[num];
			io->recv_data(vr, num*sizeof(block));
			io->recv_data(mr, num*sizeof(block));
			for(int i = 0; i < num; ++i) {
				gfmul(vr[i], delta, &vr[i]);
				vr[i] = vr[i] ^ mac[i];
				if(memcmp(vr+i, mr+i, 16) != 0) {
					std::cout << i << std::endl;
					abort();
				}
			}
		}
	}
};
}
#endif
