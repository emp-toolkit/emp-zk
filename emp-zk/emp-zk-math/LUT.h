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


class LUTIntFp {
public:
	
    int party;  

    // public 
	vector<uint64_t> writes_index;
	vector<uint64_t> writes_value;
	vector<uint64_t> writes_version;

	// Alice
	vector<uint64_t> latest_version;  

    
	vector<IntFp> writesMAC_index; 
	vector<IntFp> writesMAC_value;
	vector<IntFp> writesMAC_version;
	
	vector<IntFp> readsMAC_index;
	vector<IntFp> readsMAC_value;
	vector<IntFp> readsMAC_version;
	// public index in the check phase
	vector<uint64_t> readsnoMAC_index;
	vector<uint64_t> readsnoMAC_value;
    // 
	uint64_t read_step = 0;

	// batch check size
	uint64_t batch_check_size = 100000;

	LUTIntFp(int _party) : party(_party){}

	~LUTIntFp(){
		if(read_step != 0) 
			LUTcheck();
	}

// LUTinit(): initiate the list writes, including version, value, and index; initiate a vector latest_version
	void LUTinit(vector<uint64_t> &data)
	{ 
		// data = Value in the public LUT（key-value pairs）
		for (int i = 0; i < data.size(); i++)
		{
			writes_version.push_back(0);
			writes_value.push_back(data[i]);
			writes_index.push_back(i);

            if (party == ALICE)
				latest_version.push_back(0);
		}
	}

// LUTDisInit(): initiate the list - Discontinuous index
	void LUTDisInit(vector<uint64_t> &index, vector<uint64_t> &data)
	{ 
		assert(index.size() == data.size());
		// data = Value in the public LUT（key-value pairs）
		for (int i = 0; i < data.size(); i++)
		{
			writes_version.push_back(0);
			writes_value.push_back(data[i]);
			writes_index.push_back(index[i]);

            if (party == ALICE)
				latest_version.push_back(0);
		}
	}

// LUTread(): input (IntFp) index, (IntFp) value
	void LUTread(IntFp &index, IntFp &value)
	{
		uint64_t clear_index = 0;
		uint64_t version = 0;

		if (party == ALICE)
		{
			clear_index = (uint64_t)HIGH64(index.value);  // clear_index
			version = latest_version[clear_index];
		}

		IntFp version_read = IntFp(version, ALICE);

		readsMAC_value.push_back(value);
		readsMAC_index.push_back(index);
		readsMAC_version.push_back(version_read);

		writesMAC_value.push_back(value);
		writesMAC_index.push_back(index);
		writesMAC_version.push_back(version_read + 1);

        if (party == ALICE){
			latest_version[clear_index] = version + 1;
		}

		++read_step;

		if (read_step == batch_check_size){  
			LUTcheck();  
			read_step = 0;
		} 
	}

// LUTDisRead(): input (IntFp) index, (IntFp) value
	void LUTDisRead(IntFp &index, IntFp &value)
	{
		uint64_t clear_index = 0;
		uint64_t version = 0;
		uint64_t index_version = 0;

		if (party == ALICE)
		{
			clear_index = (uint64_t)HIGH64(index.value);  // clear_index
			for (int i = 0; i < writes_index.size(); i++){
				if (writes_index[i] == clear_index){
					index_version = i;
					break;
				}
			}
			version = latest_version[index_version];
		}

		IntFp version_read = IntFp(version, ALICE);

		readsMAC_value.push_back(value);
		readsMAC_index.push_back(index);
		readsMAC_version.push_back(version_read);

		writesMAC_value.push_back(value);
		writesMAC_index.push_back(index);
		writesMAC_version.push_back(version_read + 1);

        if (party == ALICE){
			latest_version[index_version] = version + 1;
		}

		++read_step;

		if (read_step == batch_check_size){  
			LUTcheck();  
			read_step = 0;
		} 
	}


// LUT_Mac_vector_inn_prdt(): each element in MacedList should + r, and then perform element-wise multiplication
	IntFp LUT_Mac_vector_inn_prdt(vector<IntFp> &MacedList, uint64_t r)
	{
		IntFp out = MacedList[0] + r;
		for (int i = 1; i < MacedList.size(); i++){
			out = out * (MacedList[i] + r);
		}

		return out;
	}

// LUTcheck_permutation(): check whether reads is a permutation of writes
	void LUTcheck_permutation(vector<IntFp> &readsMac, vector<uint64_t> &writesNoMac, vector<IntFp> &writesMac, uint64_t r)
	{ 
        // convert readsMac and writesMac to polynomials and evaluate the result for input r
		IntFp readsOut = LUT_Mac_vector_inn_prdt(readsMac, r);
		IntFp writesOut = LUT_Mac_vector_inn_prdt(writesMac, r);

		// add no-MAC parts in the writesOut
		uint64_t writestmp = add_mod(writesNoMac[0], r);
		for (int i = 1; i < writesNoMac.size(); i++){
			writestmp = mult_mod(writestmp, add_mod(writesNoMac[i], r));
		}

		writesOut = writesOut * writestmp;

		// checkZero
		IntFp checkzero = readsOut + writesOut.negate(); 
        checkzero.reveal_zero(); 
	}

// LUTcheck() needs parameters：readsMAC_XXX, writesMAC_XXX, writes_XXX; batch check the correctness of read-operation 
	void LUTcheck()
	{
		uint64_t tmp_version;

		// the n elements that are not in reads need to be completed
		for (int i = 0; i < writes_index.size(); i++)
		{
			readsnoMAC_index.push_back(writes_index[i]);
			readsnoMAC_value.push_back(writes_value[i]);
            
            if (party == ALICE){
				tmp_version = latest_version[i];
			}
			readsMAC_version.push_back(IntFp(tmp_version, ALICE));
		}

		assert((readsMAC_index.size() + readsnoMAC_index.size()) == (writes_index.size() + writesMAC_index.size()));

		uint64_t *a = new uint64_t[4];  // 
		__uint128_t *randomness = new __uint128_t[4]; 
	    PRG prg(fix_key);
		prg.random_block((block*)randomness, 4);
        for (int i = 0; i < 4; i++){
			a[i] = randomness[i] % PR;
		}

		// Packing writes
		vector<uint64_t> writesNoMacPackList;
		vector<IntFp> writesPackList;
		uint64_t tNoMacPack = 0;
		IntFp tPack; // 
		for (int i = 0; i < writes_index.size(); i++)
		{
			tNoMacPack = mod(mult_mod(writes_index[i], a[0]) +  mult_mod(writes_value[i], a[1]) + mult_mod(writes_version[i], a[2]));
			writesNoMacPackList.push_back(tNoMacPack);
		}

		for (int i = 0; i < writesMAC_index.size(); i++)
		{
			tPack = writesMAC_index[i] * a[0] + writesMAC_value[i] * a[1] + writesMAC_version[i] * a[2];
			writesPackList.push_back(tPack); 
		}

		// Packing reads
		vector<IntFp> readsPackList;
		for (int i = 0; i < readsMAC_index.size(); i++)
		{
			tPack = readsMAC_index[i] * a[0] + readsMAC_value[i] * a[1] + readsMAC_version[i] * a[2];
			readsPackList.push_back(tPack); 
		}
		for (int i = 0; i < readsnoMAC_index.size(); i++)
		{
			tPack = readsMAC_version[readsMAC_index.size() + i] * a[2] + add_mod(mult_mod(readsnoMAC_index[i], a[0]), mult_mod(readsnoMAC_value[i], a[1]));
			readsPackList.push_back(tPack);
		}

		assert(readsPackList.size() == (writesNoMacPackList.size() + writesPackList.size()));

		LUTcheck_permutation(readsPackList, writesNoMacPackList, writesPackList, a[3]);

		// resize reads and writesMac
		readsMAC_index.resize(0);
		readsMAC_value.resize(0);
		readsMAC_version.resize(0);
		writesMAC_index.resize(0);
		writesMAC_value.resize(0);
		writesMAC_version.resize(0);
		readsnoMAC_index.resize(0);
		readsnoMAC_value.resize(0);

		if (party == ALICE){
			std::fill(latest_version.begin(), latest_version.end(), 0);
		}

		read_step = 0;
	}
};