#pragma once 

#include "emp-zk/emp-zk-arith/zk_fp_exec.h"
#include "emp-zk/emp-zk-arith/zk_fp_exec_prover.h"
#include "emp-zk/emp-zk-arith/zk_fp_exec_verifier.h"
#include "emp-zk/emp-zk-arith/triple_auth.h"
#include "emp-zk/emp-zk-arith/ostriple.h"
#include "emp-zk/emp-zk-arith/int_fp.h"
#include "emp-zk/emp-zk-arith/conversion.h"
#include "emp-zk/emp-zk-arith/polynomial.h"

#include "emp-zk/emp-vole/utility.h"  

class LUTTwoValueIntFp {
public:

    int party;  

	vector<uint64_t> writes_index;
	vector<uint64_t> writes_a;
	vector<uint64_t> writes_b;
	vector<uint64_t> writes_version;

	vector<uint64_t> latest_version;  

	vector<IntFp> writesMAC_index; 
	vector<IntFp> writesMAC_a;
	vector<IntFp> writesMAC_b;
	vector<IntFp> writesMAC_version;
	// 
	vector<IntFp> readsMAC_index;
	vector<IntFp> readsMAC_a;
	vector<IntFp> readsMAC_b;
	vector<IntFp> readsMAC_version;
	//
	vector<uint64_t> readsnoMAC_index;
	vector<uint64_t> readsnoMAC_a;
	vector<uint64_t> readsnoMAC_b;
    // 
	uint64_t read_step = 0;

	// batch check size
	uint64_t batch_check_size = 100000;

	LUTTwoValueIntFp(int _party) : party(_party){}

	~LUTTwoValueIntFp(){
		if(read_step != 0) 
			LUTTwoValuecheck();
	}


	void LUTTwoValueinit(vector<uint64_t> &a, vector<uint64_t> &b)
	{
		assert(a.size() == b.size()); 
		for (int i = 0; i < a.size(); i++)
		{
			writes_version.push_back(0);
			writes_index.push_back(i);

			writes_a.push_back(a[i]);    
			writes_b.push_back(b[i]);

            if (party == ALICE)
				latest_version.push_back(0);
		}
	}


	void LUTTwoValueread(IntFp &index, IntFp &a, IntFp &b)
	{
		uint64_t clear_index = 0;
		uint64_t version = 0;

		if (party == ALICE){
			clear_index = (uint64_t)HIGH64(index.value);  
			version = latest_version[clear_index];
		}

		IntFp version_read = IntFp(version, ALICE);

		readsMAC_a.push_back(a);
		readsMAC_b.push_back(b);
		readsMAC_index.push_back(index);
		readsMAC_version.push_back(version_read);

		writesMAC_a.push_back(a);
		writesMAC_b.push_back(b);
		writesMAC_index.push_back(index);
		writesMAC_version.push_back(version_read + 1);

        if (party == ALICE){
			latest_version[clear_index] = version + 1;
		}

		++read_step;

		if (read_step == batch_check_size){  
			LUTTwoValuecheck();    
			read_step = 0;
		}
	}


	IntFp LUT_Mac_vector_inn_prdt(vector<IntFp> &MacedList, uint64_t r)
	{
		IntFp out = MacedList[0] + r;
		for (int i = 1; i < MacedList.size(); i++){
			out = out * (MacedList[i] + r);
		}

		return out;
	}


	void LUTTwoValuecheck_permutation(vector<IntFp> &readsMac, vector<uint64_t> &writesNoMac, vector<IntFp> &writesMac, uint64_t r)
	{ 
		IntFp readsOut = LUT_Mac_vector_inn_prdt(readsMac, r);
		IntFp writesOut = LUT_Mac_vector_inn_prdt(writesMac, r);

		uint64_t writestmp = add_mod(writesNoMac[0], r);
		for (int i = 1; i < writesNoMac.size(); i++){
			writestmp = mult_mod(writestmp, add_mod(writesNoMac[i], r));
		}

		writesOut = writesOut * writestmp;

		// checkZero
		IntFp checkzero = readsOut + writesOut.negate(); 
        checkzero.reveal_zero(); 
	}


	void LUTTwoValuecheck()
	{
		uint64_t tmp_version;

		for (int i = 0; i < writes_index.size(); i++)
		{
			readsnoMAC_index.push_back(i);
			readsnoMAC_a.push_back(writes_a[i]);
			readsnoMAC_b.push_back(writes_b[i]);
            
            if (party == ALICE){
				tmp_version = latest_version[i];
			}
			readsMAC_version.push_back(IntFp(tmp_version, ALICE));
		}

		assert((readsMAC_index.size() + readsnoMAC_index.size()) == (writes_index.size() + writesMAC_index.size()));

        // generate randomness
		uint64_t *a = new uint64_t[5];  // 
		__uint128_t *randomness = new __uint128_t[5]; 
	    PRG prg(fix_key);
		prg.random_block((block*)randomness, 5);
        for (int i = 0; i < 5; i++){
			a[i] = randomness[i] % PR;
		}

		// Packing writes
		vector<uint64_t> writesNoMacPackList;
		vector<IntFp> writesPackList;
		uint64_t tNoMacPack = 0;
		IntFp tPack; // 
		for (int i = 0; i < writes_index.size(); i++)
		{
			tNoMacPack = mod(mult_mod(writes_index[i], a[0]) +  mult_mod(writes_a[i], a[1]) +  mult_mod(writes_b[i], a[2]) + mult_mod(writes_version[i], a[3]));
			writesNoMacPackList.push_back(tNoMacPack);
		}
		for (int i = 0; i < writesMAC_index.size(); i++)
		{
			tPack = writesMAC_index[i] * a[0] + writesMAC_a[i] * a[1] + writesMAC_b[i] * a[2] + writesMAC_version[i] * a[3];
			writesPackList.push_back(tPack); 
		}

		// Packing reads
		vector<IntFp> readsPackList;
		for (int i = 0; i < readsMAC_index.size(); i++)
		{
			tPack = readsMAC_index[i] * a[0] + readsMAC_a[i] * a[1] + readsMAC_b[i] * a[2] + readsMAC_version[i] * a[3];
			readsPackList.push_back(tPack); 
		}
		for (int i = 0; i < readsnoMAC_index.size(); i++)
		{
			tPack = readsMAC_version[readsMAC_index.size() + i] * a[3] + mod(mult_mod(readsnoMAC_index[i], a[0]) + mult_mod(readsnoMAC_a[i], a[1]) + mult_mod(readsnoMAC_b[i], a[2]));
			readsPackList.push_back(tPack);
		}

		assert(readsPackList.size() == (writesNoMacPackList.size() + writesPackList.size()));

		LUTTwoValuecheck_permutation(readsPackList, writesNoMacPackList, writesPackList, a[4]);

		// resize reads and writesMac
		readsMAC_index.resize(0);
		readsMAC_a.resize(0);
		readsMAC_b.resize(0);
		readsMAC_version.resize(0);

		writesMAC_index.resize(0);
		writesMAC_a.resize(0);
		writesMAC_b.resize(0);
		writesMAC_version.resize(0);

		readsnoMAC_index.resize(0);
		readsnoMAC_a.resize(0);
		readsnoMAC_b.resize(0);

		if (party == ALICE){
			std::fill(latest_version.begin(), latest_version.end(), 0);
		}

		read_step = 0;
	}
};