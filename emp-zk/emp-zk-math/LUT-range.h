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


class LUTRangeIntFp
{
public:

	int party;

	vector<uint64_t> writes_index;
	vector<uint64_t> writes_version;

	vector<uint64_t> latest_version;

	vector<IntFp> writesMAC_index;
	vector<IntFp> writesMAC_version;
	//
	vector<IntFp> readsMAC_index;
	vector<IntFp> readsMAC_version;
	vector<uint64_t> readsnoMAC_index;
	//
	uint64_t read_step = 0;

	uint64_t simplecheck = 0;
	vector<IntFp> batch_simplecheck;

	// batch check size
	uint64_t batch_check_size = 100000;

	LUTRangeIntFp(int _party) : party(_party) {}

	~LUTRangeIntFp() {
		if(read_step != 0) 
			LUTRangecheck();
	}

	// LUTRangeinit(): initiate the list writes, including version, and index; initiate a vector latest_version
	void LUTRangeinit(uint64_t lutSize)
	{
		for (int i = 0; i < lutSize; i++)
		{
			writes_version.push_back(0);
			writes_index.push_back(i);

			if (party == ALICE)
				latest_version.push_back(0);
		}
	}

	// LUTRangeread(): input (IntFp) index
	void LUTRangeread(IntFp &index)
	{
		uint64_t clear_index = 0;
		uint64_t version = 0;

		if (party == ALICE){
			clear_index = (uint64_t)HIGH64(index.value); 
			version = latest_version[clear_index];
		}

		IntFp version_read = IntFp(version, ALICE);

		readsMAC_index.push_back(index);
		readsMAC_version.push_back(version_read);

		writesMAC_index.push_back(index);
		writesMAC_version.push_back(version_read + 1);

		if (party == ALICE){
			latest_version[clear_index] = version + 1;
		}

		++read_step;

		if (read_step == batch_check_size) {
			LUTRangecheck();					 
			read_step = 0;
		}
	}

	// LUTRange_Mac_vector_inn_prdt(): each element in MacedList should + r, and then perform element-wise multiplication
	IntFp LUTRange_Mac_vector_inn_prdt(vector<IntFp> &MacedList, uint64_t r)
	{
		IntFp out = MacedList[0] + r;
		for (int i = 1; i < MacedList.size(); i++){
			out = out * (MacedList[i] + r);
		}

		return out;
	}

	// LUTRangecheck_permutation(): check whether reads is a permutation of writes
	void LUTRangecheck_permutation(vector<IntFp> &readsMac, vector<uint64_t> &writesNoMac, vector<IntFp> &writesMac, uint64_t r)
	{
		IntFp readsOut = LUTRange_Mac_vector_inn_prdt(readsMac, r);
		IntFp writesOut = LUTRange_Mac_vector_inn_prdt(writesMac, r);

		uint64_t writestmp = add_mod(writesNoMac[0], r);
		for (int i = 1; i < writesNoMac.size(); i++){
			writestmp = mult_mod(writestmp, add_mod(writesNoMac[i], r));
		}

		writesOut = writesOut * writestmp;

		// checkZero
		IntFp checkzero = readsOut + writesOut.negate();

		checkzero.reveal_zero(); 
	}

	// LUTRangecheck() needs parameters：readsMAC_XXX, writesMAC_XXX, writes_XXX; batch check the correctness of read-operation
	void LUTRangecheck()
	{
		uint64_t tmp_version;

		for (int i = 0; i < writes_index.size(); i++)
		{
			readsnoMAC_index.push_back(i);

			if (party == ALICE){
				tmp_version = latest_version[i];
			}
			readsMAC_version.push_back(IntFp(tmp_version, ALICE));
		}

		assert((readsMAC_index.size() + readsnoMAC_index.size()) == (writes_index.size() + writesMAC_index.size()));

		uint64_t *a = new uint64_t[3]; // 
		__uint128_t *randomness = new __uint128_t[3];
		PRG prg(fix_key);
		prg.random_block((block *)randomness, 3);
		for (int i = 0; i < 3; i++){
			a[i] = randomness[i] % PR;
		}

		// Packing writes
		vector<uint64_t> writesNoMacPackList;
		vector<IntFp> writesPackList;
		uint64_t tNoMacPack = 0;
		IntFp tPack; //
		for (int i = 0; i < writes_index.size(); i++)
		{
			tNoMacPack = add_mod(mult_mod(writes_index[i], a[0]), mult_mod(writes_version[i], a[1]));
			writesNoMacPackList.push_back(tNoMacPack);
		}
		for (int i = 0; i < writesMAC_index.size(); i++)
		{
			tPack = writesMAC_index[i] * a[0] + writesMAC_version[i] * a[1];
			writesPackList.push_back(tPack);
		}

		// Packing reads
		vector<IntFp> readsPackList;
		for (int i = 0; i < readsMAC_index.size(); i++)
		{
			tPack = readsMAC_index[i] * a[0] + readsMAC_version[i] * a[1];
			readsPackList.push_back(tPack);
		}
		for (int i = 0; i < readsnoMAC_index.size(); i++)
		{
			tPack = readsMAC_version[readsMAC_index.size() + i] * a[1] + mult_mod(readsnoMAC_index[i], a[0]);
			readsPackList.push_back(tPack);
		}

		assert(readsPackList.size() == (writesNoMacPackList.size() + writesPackList.size()));

		LUTRangecheck_permutation(readsPackList, writesNoMacPackList, writesPackList, a[2]);

		// resize reads and writesMac
		readsMAC_index.resize(0);
		readsMAC_version.resize(0);
		writesMAC_index.resize(0);
		writesMAC_version.resize(0);
		readsnoMAC_index.resize(0);

		if (party == ALICE){
			std::fill(latest_version.begin(), latest_version.end(), 0);
		}

		read_step = 0;
	}

	// LUTRangecheck(): simple check - batch check zero
	void LUTRangeSimplecheck(IntFp &index)
	{
		uint64_t end = writes_index.size();
		IntFp tmp = index;
		for (int i = 1; i < end; i++){
			tmp = tmp * (index + (PR - i));
		}

		batch_simplecheck.push_back(tmp);
		++simplecheck;

		if (simplecheck == writes_index.size() * 8)
		{ 
			bool res = batch_reveal_check_zero(batch_simplecheck.data(), batch_simplecheck.size());

			if (!res)
				error("batch_reveal_check_zero failed");

			simplecheck = 0;
			batch_simplecheck.resize(0);
		}
	}
};