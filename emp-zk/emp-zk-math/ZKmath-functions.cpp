#include "emp-zk/emp-zk-math/ZKmath-functions.h"

void ZKPositiveDigDec(int party, IntFp *x, IntFp *y, uint64_t *digit_size, int num_digits, int dim)  // x:dim；y:num_digits*dim; num_digits: the number of sigits，digit_size: the size of each digit
{
	uint64_t curr_x = 0;
	uint64_t ori_y = 0;
	IntFp *sum_ydigits = new IntFp[dim];  // for prove
	vector<IntFp> zero;

	// step1: compute y + check range
	for (int i = 0; i < dim; i++){
		// step 1: Digital decomposition of the lowest bit
		int shiftlen = 0;
		uint64_t curr_digitlen = digit_size[0];
		uint64_t digit_mask = (1ULL << curr_digitlen) - 1;
		if (party == ALICE){
			curr_x = (uint64_t)HIGH64(x[i].value);
			ori_y = (curr_x >> shiftlen) & digit_mask; // The low-order digit is placed here
		}
		sum_ydigits[i] = IntFp(ori_y, ALICE);
		y[i * num_digits] = sum_ydigits[i]; 

		// step 2: check the range of the lowest digit
		if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
			LUTRange[curr_digitlen]->LUTRangeread(y[i * num_digits]);
		} else if (curr_digitlen == 1){
			IntFp r = (y[i * num_digits].negate() + 1) * y[i * num_digits];
			zero.push_back(r);
		} else {
			error("Incorrect decomposition length");
		}

		for (int j = 1; j < num_digits; j++){
			// step 3: Decompose the remaining digits
			curr_digitlen = digit_size[j];
			digit_mask = (1ULL << curr_digitlen) - 1;
			shiftlen += digit_size[j - 1];
			if (party == ALICE){
				ori_y = (curr_x >> shiftlen) & digit_mask; // The low-order digit is placed here
			}
			IntFp y_digit = IntFp(ori_y, ALICE);
			y[i * num_digits + j] = y_digit; 

			// step 4: checkrange
			if (curr_digitlen > 1 && curr_digitlen < NUM_RANGE){
				LUTRange[curr_digitlen]->LUTRangeread(y_digit);
			} else if (curr_digitlen == 1){
				IntFp r = (y_digit.negate() + 1) * y_digit;
				zero.push_back(r);
			} else {
				error("Incorrect decomposition length");
			}

			// step 4: sum
			sum_ydigits[i] = sum_ydigits[i] + y_digit * (1ULL << shiftlen);
		}

		// step 5: checkzero
		IntFp z = x[i] + sum_ydigits[i].negate();
		zero.push_back(z);
	}

	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("step 2: batch_reveal_check_zero failed");

	delete[] sum_ydigits;
}

void ZKPositiveDigDecAny(int party, IntFp *x, IntFp *xhigh, IntFp *xlow, uint64_t highsize, uint64_t lowsize, int dim)    // Decompose into any size x: dim; xhigh: dim; ylow: dim
{
	uint64_t k = ceil(log2(PR));
	assert(highsize + lowsize < k);

	// step 1: Processing decomposition size
	uint64_t high_num_digit = ceil((double)highsize/(NUM_RANGE - 1));
	uint64_t most_highsize = highsize - (high_num_digit - 1) * (NUM_RANGE - 1);
	
	uint64_t low_num_digit = ceil((double)lowsize/(NUM_RANGE - 1));
	uint64_t most_lowsize = lowsize - (low_num_digit - 1) * (NUM_RANGE - 1);

	// step 2: construct uint64_t *digit_size and int num_digits
	uint64_t num_digits = high_num_digit + low_num_digit;
	uint64_t *digit_size = new uint64_t[num_digits];
	for (int i = 0; i < low_num_digit - 1; i++){
		digit_size[i] = NUM_RANGE - 1;
	}
	digit_size[low_num_digit - 1] = most_lowsize;
	for (int i = low_num_digit; i < num_digits - 1; i++){
		digit_size[i] = NUM_RANGE - 1;
	}
	digit_size[num_digits - 1] = most_highsize;

	// step 3: invoke ZKPositiveDigDec()
	IntFp *y_digits = new IntFp[dim * num_digits];
	ZKPositiveDigDec(party, x, y_digits, digit_size, num_digits, dim);

	// step 4: construct y
	for (int i = 0; i < dim; i++){
		// Deal with the low bits
		int shiftlen = 0;
		xlow[i] = y_digits[i * num_digits];
		for (int j = 1; j < low_num_digit; j++){
			shiftlen += digit_size[j - 1];
			xlow[i] = xlow[i] + y_digits[i * num_digits + j] * (1ULL << shiftlen);
		}

		// then high bits
		shiftlen = 0;
		xhigh[i] = y_digits[i * num_digits + low_num_digit];
		for (int j = 1; j < high_num_digit; j++){
			shiftlen += digit_size[low_num_digit + j - 1];
			xhigh[i] = xhigh[i] + y_digits[i * num_digits + low_num_digit + j] * (1ULL << shiftlen);
		}
	}
	delete[] digit_size;
	delete[] y_digits;
}

void ZKpositiveTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen)   
{
	if (trunclen == 0){
		for (int i = 0; i < dim; i++){
			y[i] = x[i];
		}
	}else{
		uint64_t m = ceil(log2(PR)) - 1;
		IntFp *ylow = new IntFp[dim];
		ZKPositiveDigDecAny(party, x, y, ylow, m - trunclen, trunclen, dim);

		delete[] ylow;
	}
	
}

void ZKgeneralTruncAny(int party, IntFp *x, IntFp *y, int dim, uint64_t trunclen)   
{
	// step 1: cmp
	IntFp *b = new IntFp[dim];
	uint64_t constant = (PR + 1)/2;
	ZKcmpPositive(party, x, constant, b, dim);

	// step 2: line 2
	IntFp *z = new IntFp[dim];
	IntFp *zTrunc = new IntFp[dim];
	IntFp *t1 = new IntFp[dim];
	IntFp *t2 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t1[i] = b[i] * 2 + (PR -1);
		t2[i] = b[i].negate() + 1;
		z[i] = t1[i] * x[i] + t2[i].negate();
	}
	ZKpositiveTruncAny(party, z, zTrunc, dim, trunclen);

	// step 3: line 3
	for (int i = 0; i < dim; i++){
		y[i] = t1[i] * zTrunc[i] + t2[i].negate();
	}

	delete[] b;
	delete[] z;
	delete[] zTrunc;
	delete[] t1;
	delete[] t2;
}

void ZKcmpRealVrfyPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == (PR+1)/2);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	// cout << "FINIAL_CMP_LUT_NUM: " << FINIAL_CMP_LUT_NUM  << endl;
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);
		}
		if (party == ALICE){
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t y_field = 0;
	IntFp sum_z;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTvrfyCmpLx[j]->writes_value[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx
			LUTvrfyCmpLx[j]->LUTread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j]);
		}

		// step 6: compute sum_z
		sum_z = z[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
		}

		// step 7: compute y
		if (party == ALICE){
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);

		// step 9: compute y - 1  for checkzero
		zero.push_back(y[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] z;
}

void ZKcmpRealVrfyP(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == PR);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);
		}
		if (party == ALICE){
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t y_field = 0;
	IntFp sum_z;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTvrfyCmpLx_InP[j]->writes_value[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx_InP
			LUTvrfyCmpLx_InP[j]->LUTread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j]);
		}

		// step 7: compute sum_z
		sum_z = z[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
		}

		// step 6: compute y
		if (party == ALICE){
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);

		// step 9: compute y - 1  for checkzero
		zero.push_back(y[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] z;
}

void ZKcmpPositive(int party, IntFp *x, uint64_t c, IntFp *y, int dim)
{
	assert(c == (PR+1)/2);
	vector<IntFp> zero;

	// step 1: decompose x
	IntFp *x_digit = new IntFp[dim * FINIAL_CMP_LUT_NUM];
	uint64_t x_field = 0;
	uint64_t digit_field = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			x_field = (uint64_t)HIGH64(x[i].value);
		}
		for (int j = 0; j < FINIAL_CMP_LUT_NUM - 1; j++){
			if (party == ALICE){
				digit_field = (x_field >> (j * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1ULL);
			}
			x_digit[i * FINIAL_CMP_LUT_NUM + j] = IntFp(digit_field, ALICE);			// [x_j]_p
		}
		if (party == ALICE){
			// msb
			digit_field = (x_field >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);
		}
		x_digit[i * FINIAL_CMP_LUT_NUM + (FINIAL_CMP_LUT_NUM - 1)] = IntFp(digit_field, ALICE);
	}

	// step 2: compute t for checkzero
	IntFp *t = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		t[i] = x_digit[i * FINIAL_CMP_LUT_NUM];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			t[i] = t[i] + x_digit[i * FINIAL_CMP_LUT_NUM + j] * (1ULL << (j * CMP_DIGIT_LEN));
		}
		zero.push_back(t[i] + x[i].negate());
	}

	// step 3: decompose c 
	uint64_t *c_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		c_digit[i] = (c >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	c_digit[FINIAL_CMP_LUT_NUM - 1] = (c >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: decompose PR 
	uint64_t *p_digit = new uint64_t[FINIAL_CMP_LUT_NUM];
	for (int i = 0; i < FINIAL_CMP_LUT_NUM - 1; i++){
		p_digit[i] = (PR >> (i * CMP_DIGIT_LEN)) & ((1ULL << CMP_DIGIT_LEN) - 1);
	}
	p_digit[FINIAL_CMP_LUT_NUM - 1] = (PR >> ((FINIAL_CMP_LUT_NUM - 1) * CMP_DIGIT_LEN)) & ((1ULL << CMP_LAST_DIGIT_BITLEN) - 1);

	// step 4: compute z and u 
	IntFp *z = new IntFp[FINIAL_CMP_LUT_NUM];
	IntFp *u = new IntFp[FINIAL_CMP_LUT_NUM];
	IntFp *v = new IntFp[dim];
	uint64_t eq = 0;
	uint64_t lt = 0;
	uint64_t z_field = 0;
	uint64_t u_field = 0;
	uint64_t y_field = 0;
	uint64_t v_field = 0;
	IntFp sum_z;
	IntFp sum_u;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < FINIAL_CMP_LUT_NUM; j++){
			// step 4: compute z and u
			if(party == ALICE){
				digit_field = (uint64_t)HIGH64(x_digit[i * FINIAL_CMP_LUT_NUM + j].value);
				z_field = LUTCmpLx[j]->writes_a[digit_field];
				u_field = LUTCmpLx[j]->writes_b[digit_field];
			}
			z[j] = IntFp(z_field, ALICE);
			u[j] = IntFp(u_field, ALICE);

			// step 5: Lookup - LUTvrfyCmpLx_InP
			LUTCmpLx[j]->LUTTwoValueread(x_digit[i * FINIAL_CMP_LUT_NUM + j], z[j], u[j]);
		}

		// step 7: compute sum_z and sum_u
		sum_z = z[0];
		sum_u = u[0];
		for (int j = 1; j < FINIAL_CMP_LUT_NUM; j++){
			sum_z = sum_z + z[j];
			sum_u = sum_u + u[j];
		}

		// step 6: compute y and v
		if (party == ALICE){
			int64_t correct_index_y = -1;
			int64_t correct_index_v = -1;
			uint64_t sum_z_field = (uint64_t)HIGH64(sum_z.value);
			uint64_t sum_u_field = (uint64_t)HIGH64(sum_u.value);
			for (int j = 0; j < LUTvrfyCmpLy->writes_index.size(); j++){
				if (LUTvrfyCmpLy->writes_index[j] == sum_z_field){
					y_field = LUTvrfyCmpLy->writes_value[j];
					correct_index_y = j;
				}
				if (LUTvrfyCmpLy->writes_index[j] == sum_u_field){
					v_field = LUTvrfyCmpLy->writes_value[j];
					correct_index_v = j;
				}
				if (correct_index_y > (-1) &&  correct_index_v > (-1)){
					break;
				}
			}
		}
		y[i] = IntFp(y_field, ALICE);
		v[i] = IntFp(v_field, ALICE);

		// step 8: Lookup - LUTvrfyCmpLy
		LUTvrfyCmpLy->LUTDisRead(sum_z, y[i]);
		LUTvrfyCmpLy->LUTDisRead(sum_u, v[i]);

		// step 9: compute v - 1  for checkzero
		zero.push_back(v[i] + (PR - 1));
	}

	// step 10: checkzero
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] x_digit;
	delete[] t;
	delete[] c_digit;
	delete[] p_digit;
	delete[] z;
	delete[] u;
	delete[] v;
}

void ZKMax(int party, IntFp *x, IntFp *y, int rows, int cols)    // max in each rows; x: rows*cols  y:rows
{ 
	// step 1: Prover generate y
	uint64_t x_max = 0;
	for (int i = 0; i < rows; i++){
		if (party == ALICE){
			x_max = (uint64_t)HIGH64(x[i * cols].value);
			for (int j = 1; j < cols; j++){
				uint64_t x_field = (uint64_t)HIGH64(x[i * cols + j].value);
				// real value comparison; need to re-fresh x_max
				if ((x_max <= (PR-1)/2) && (x_field <= (PR-1)/2) || (x_max > (PR-1)/2) && (x_field > (PR-1)/2)){
					if (x_field > x_max){
						x_max = x_field;
					}
				} else if ((x_max > (PR-1)/2) && (x_field <= (PR-1)/2)){
					x_max = x_field;
				}
			}
		}
		y[i] = IntFp(x_max, ALICE);
	}

	// step 2: compute t = x_max - x
	IntFp *t = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			t[i * cols + j] = y[i] + x[i * cols + j].negate();
		}
	}

	// step 3: invoke comparison
	IntFp *b = new IntFp[rows * cols];
	uint64_t constant = (PR+1)/2;
	ZKcmpRealVrfyPositive(party, t, constant, b, rows * cols);

	// step 4: multiplication and truncation
	IntFp *d = new IntFp[rows];
	for (int i = 0; i < rows; i++){
		d[i] = t[i * cols];
	}
	for (int i = 1; i < cols; i++){
		for (int j = 0; j < rows; j++){
			d[j] = d[j] * t[j * cols + i];
		}
	}

	// checkzero
	vector<IntFp> zero;
	for (int i = 0; i < rows; i++){
		zero.push_back(d[i]);
		for (int j = 0; j < cols; j++){
			zero.push_back(b[i * cols + j] + (PR-1));
		}
	}
	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] t;
	delete[] b;
	delete[] d;
}


void ZKmsnzb(int party, IntFp *x, IntFp *y, int dim)
{
	uint64_t msnzb = 0;
	uint64_t z0_field = 0;
	uint64_t z1_field = 0;
	IntFp *z0 = new IntFp[dim];	
	IntFp *z1 = new IntFp[dim];

	// step 1: compute msnzb, then checkLUT
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			uint64_t curr_x = (uint64_t)HIGH64(x[i].value);
			msnzb = floor(log2(curr_x));   // 
			z0_field = LUTmsnzb2value->writes_a[msnzb];
			z1_field = LUTmsnzb2value->writes_b[msnzb];
		}
		y[i] = IntFp(msnzb, ALICE);
		z0[i] = IntFp(z0_field, ALICE);
		z1[i] = IntFp(z1_field, ALICE);
        LUTmsnzb2value->LUTTwoValueread(y[i], z0[i], z1[i]);
	}

	// step 2: cmp
	IntFp *b0 = new IntFp[dim];
	IntFp *b1 = new IntFp[dim];
	IntFp *in0 = new IntFp[dim];
	IntFp *in1 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		in0[i] = x[i] + z0[i].negate();
		in1[i] = z1[i] + x[i].negate();
	}
	uint64_t constant = (PR+1)/2;
	ZKcmpRealVrfyPositive(party, in0, constant, b0, dim);
	ZKcmpRealVrfyPositive(party, in1, constant, b1, dim);

	// step 3: checkzero
	vector<IntFp> zero;
	for (int i = 0; i < dim; i++){
		zero.push_back(b0[i] + (PR - 1));
		zero.push_back(b1[i] + (PR - 1));
	}

	bool res = batch_reveal_check_zero(zero.data(), zero.size());
	if (!res)
		error("batch_reveal_check_zero failed");

	delete[] z0;
	delete[] z1;
	delete[] b0;
	delete[] b1;
	delete[] in0;
	delete[] in1;
}

void ZKExtend(int party, IntFp *x, IntFp *k, IntFp *y, int dim)    // For Div
{
	for (int i = 0; i < dim; i++){
		// step 1: compute mz
		uint64_t z = 0;
		if (party == ALICE){
			uint64_t curr_k = (uint64_t)HIGH64(k[i].value);
			z = 1ULL << curr_k;
		}

		IntFp mz = IntFp(z, ALICE);
		LUTextend->LUTread(k[i], mz);

		// step 2: compute y
		y[i] = x[i] * mz;
	}
}

void ZKExtendSqrt(int party, IntFp *x, IntFp *k, IntFp *y, int dim)    // For rSqrt
{
	for (int i = 0; i < dim; i++){
		// step 1: compute mz
		uint64_t z = 0;
		if (party == ALICE){
			uint64_t curr_k = (uint64_t)HIGH64(k[i].value);
			z = LUTsqrtExtend->writes_value[curr_k];
		}

		IntFp mz = IntFp(z, ALICE);
		LUTsqrtExtend->LUTread(k[i], mz);

		// step 2: compute y
		y[i] = x[i] * mz;
	}
}

void ZKExp(int party, IntFp *x, IntFp *y, int dim)  // Exp for each element in the vector 
{
	// step 1: DigitDec
	IntFp *xDigDec = new IntFp[dim * EXP_LUT_NUM];
	uint64_t *digit_size = new uint64_t[EXP_LUT_NUM];
	for (int i = 0; i < EXP_LUT_NUM - 1; i++){
		digit_size[i] = EXP_DIGIT_LEN;
	}
	digit_size[EXP_LUT_NUM - 1] = EXP_N - EXP_DIGIT_LEN * (EXP_LUT_NUM - 1);
	ZKPositiveDigDec(party, x, xDigDec, digit_size, EXP_LUT_NUM, dim);

	// step 2: LUTexp
	IntFp *yDigDec = new IntFp[dim * EXP_LUT_NUM];
	uint64_t digit_field = 0;
	uint64_t value_field = 0;
	for (int i = 0; i < dim; i++){
		for (int j = 0; j < EXP_LUT_NUM; j++){
			IntFp digit = xDigDec[i * EXP_LUT_NUM + j];
			if (party == ALICE){
				digit_field = (uint64_t)HIGH64(digit.value);
				value_field = LUTexp[j]->writes_value[digit_field];
			}
			yDigDec[i * EXP_LUT_NUM + j] = IntFp(value_field, ALICE);
			LUTexp[j]->LUTread(digit, yDigDec[i * EXP_LUT_NUM + j]);
		}

		y[i] = yDigDec[i * EXP_LUT_NUM];
	}

	// step 3: multiplication + truncation
	for (int i = 1; i < EXP_LUT_NUM; i++){
		for (int j = 0; j < dim; j++){
			y[j] = y[j] * yDigDec[j * EXP_LUT_NUM + i];
		}
		ZKpositiveTruncAny(party, y, y, dim, SCALE);
	}

	delete[] xDigDec;
	delete[] digit_size;
	delete[] yDigDec;
}

void ZKDiv(int party, IntFp *x, IntFp *y, int dim)  //y=1/x
{
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(party, x, k, dim);
	
	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (DIV_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(party, x, extendLen, z, dim);  

	// step 3: DigitDec
	for (int i = 0; i < dim; i++){
		z[i] = z[i] + (PR - (1ULL << (DIV_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, z, z1, z0, DIV_M, DIV_N - 1 - DIV_M, dim);

	// step 4: LUT + y'trunc
	IntFp *yprime = new IntFp[dim];
	IntFp *yprimeTrunc = new IntFp[dim];
	int32_t tmp = 1ULL << DIV_M;
	uint64_t coff1 = 0;
	uint64_t coff2 = 0;
	for (int i = 0; i < dim; i++){
		if (party == ALICE){
			// a \in (1/2, 1)
			uint64_t digit = (uint64_t)HIGH64(z1[i].value);
			coff1 = LUTdiv->writes_a[digit];
        	// b \in (1/4, 1)
			coff2 = LUTdiv->writes_b[digit];
		}
		IntFp a = IntFp(coff1, ALICE);
		IntFp b = IntFp(coff2, ALICE);
		LUTdiv->LUTTwoValueread(z1[i], a, b);

		yprime[i] = (b * z0[i]).negate() + a;
	}

	ZKpositiveTruncAny(party, yprime, yprimeTrunc, dim, DIV_N - 1);

	ZKExtend(party, yprimeTrunc, extendLen, yprime, dim);  // re-use yprime to place the outputs of ZKExtend
	ZKpositiveTruncAny(party, yprime, y, dim, DIV_N - SCALE - 1);  

	delete[] k;
	delete[] extendLen;
	delete[] z;
	delete[] z1;
	delete[] z0;
	delete[] yprime;
	delete[] yprimeTrunc;
}

void ZKrSqrt(int party, IntFp *x, IntFp *y, int dim, int iter)
{
	cout << "ZKrSqrt - begin" << endl;
	// step 1: msnzb
	IntFp *k = new IntFp[dim];
	ZKmsnzb(party, x, k, dim);

	// step 2: extend
	IntFp *extendLen = new IntFp[dim]; 
	for (int i = 0; i < dim; i++){
		extendLen[i] = k[i].negate() + (SQRT_N - 1);
	}
	IntFp *z = new IntFp[dim];
	ZKExtend(party, x, extendLen, z, dim);

	// step 3: DigitDec
	IntFp *k1 = new IntFp[dim];
	IntFp *k0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, k, k1, k0, ceil(log2(SQRT_N)) - 1, 1, dim);
	IntFp *zprime = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		zprime[i] = z[i] + (PR - (1ULL << (SQRT_N - 1)));     
	}
	IntFp *z1 = new IntFp[dim];
	IntFp *z0 = new IntFp[dim];
	ZKPositiveDigDecAny(party, zprime, z1, z0, SQRT_M, SQRT_N - 1 - SQRT_M, dim);

	// step 4: LUTsqrt
	IntFp *abeforeTrunc = new IntFp[dim];
	IntFp *a = new IntFp[dim];
	IntFp *b = new IntFp[dim];
	IntFp *c = new IntFp[dim];
	uint64_t value = 0;
	for (int i = 0; i < dim; i++){
		IntFp sqrt_index = z1[i] * 2 + k0[i];
		if (party == ALICE){
			uint64_t index = (uint64_t)HIGH64(sqrt_index.value);
			value = LUTsqrt->writes_value[index];
		}
		IntFp mvalue = IntFp(value, ALICE);
		LUTsqrt->LUTread(sqrt_index, mvalue);

	// step 5: compute a, b, c
		abeforeTrunc[i] = (k0[i] + 1) * z[i];
		b[i] = mvalue;
		c[i] = mvalue;
	}
	ZKpositiveTruncAny(party, abeforeTrunc, a, dim, SQRT_N - 1 - SCALE);

	// step 6: iteration
	IntFp *cbeforeTrunc = new IntFp[dim];
	for (int i = 0; i < iter; i++){
		for (int j = 0; j < dim; j++){
			abeforeTrunc[j] = b[j] * b[j] * a[j];
		}
		ZKpositiveTruncAny(party, abeforeTrunc, a, dim, 2 * SCALE);
		for (int j = 0; j < dim; j++){
			b[j] = a[j].negate() + (3 * (1ULL << SCALE));  
			cbeforeTrunc[j] = c[j] * b[j];
		}
		ZKpositiveTruncAny(party, cbeforeTrunc, c, dim, SCALE + 1);
	}

	// step 7: extend and truncation
	IntFp *CI = new IntFp[dim];
	ZKExtendSqrt(party, c, k, CI, dim);
	uint64_t trunclen = (SQRT_N - SCALE)/2;
	ZKpositiveTruncAny(party, CI, y, dim, trunclen);

	delete[] k;
	delete[] extendLen;
	delete[] z;
	delete[] k1;
	delete[] k0;
	delete[] zprime;
	delete[] z1;
	delete[] z0;
	delete[] abeforeTrunc;
	delete[] a;
	delete[] b;
	delete[] c;
	delete[] cbeforeTrunc;
	delete[] CI;
}

void ZKSigmoid(int party, IntFp *x, IntFp *y, int dim)
{
	// step 1: compute whether x is positive 
	IntFp *b = new IntFp[dim];
	uint64_t constant = (PR+1)/2;
	ZKcmpPositive(party, x, constant, b, dim);

	// step 2: compute x_bar
	IntFp *x_bar = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		x_bar[i] = (b[i] * 2 + (PR - 1)) * x[i];
	}

	// step 3: compute z
	IntFp *z = new IntFp[dim];
	ZKExp(party, x_bar, z, dim);

	// step 4: coompute d1
	IntFp *d1 = new IntFp[dim];
	IntFp *zaddone = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		zaddone[i] = z[i] + 1;
	}
	ZKDiv(party, zaddone, d1, dim);

	// step 5: compute d2 and truncation
	IntFp *d2 = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		d2[i] = z[i] * d1[i];
	}
	ZKpositiveTruncAny(party, d2, d2, dim, SCALE);

	// step 6: compute y
	for (int i = 0; i < dim; i++){
		y[i] = b[i] * d1[i] + (b[i].negate() + 1) * d2[i];
	}
}

void ZKGeLU(int party, IntFp *x, IntFp *y, int dim)
{
	// step 1: compute x^3
	IntFp *k = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		k[i] = x[i] * x[i] * x[i];
	}
	ZKgeneralTruncAny(party, k, k, dim, 2 * SCALE);

	// step 2: compute t
	IntFp *t = new IntFp[dim];
	uint64_t a_field = Real2Field(sqrt(2/M_PI), SCALE);
	uint64_t b_field = Real2Field(0.044715, SCALE);
	for (int i = 0; i < dim; i++){
		t[i] = (x[i] + k[i] * b_field) * a_field;
	}
	ZKgeneralTruncAny(party, t, t, dim, 2 * SCALE);

	// step 3: compute c
	IntFp *c = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		c[i] = t[i] * 2;
	}
	ZKgeneralTruncAny(party, c, c, dim, SCALE);

	// step 4: compute sigmoid
	IntFp *d = new IntFp[dim];
	ZKSigmoid(party, c, d, dim);

	// step 5: compute z
	IntFp *z = new IntFp[dim];
	for (int i = 0; i < dim; i++){
		z[i] = d[i] * 2 + (PR - 1);
	}
	ZKgeneralTruncAny(party, z, z, dim, SCALE);

	// step 6: compute y
	uint64_t c_field = Real2Field(0.5, SCALE);
	for (int i = 0; i < dim; i++){
		y[i] = x[i] * (z[i] + 1) * c_field;
	}
	ZKgeneralTruncAny(party, y, y, dim, 2 * SCALE);

	delete[] k;
	delete[] t;
	delete[] c;
	delete[] d;
	delete[] z;
}

void ZKSoftmax(int party, IntFp *x, IntFp *y, int rows, int cols)  // x: rows*cols  y: rows*cols
{
	IntFp *max = new IntFp[rows];
	IntFp *z = new IntFp[rows * cols];
	IntFp *ez = new IntFp[rows * cols];
	IntFp *sum = new IntFp[rows];
	IntFp *t = new IntFp[rows];
	
	// step 1: Max in each rows
	ZKMax(party, x, max, rows, cols);
	cout << "finish ZKMax" << endl;

	// step 2: exp
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			z[i * cols + j] = x[i * cols + j].negate() + max[i];
		}
	}
	ZKExp(party, z, ez, rows * cols);
	cout << "finish ZKExp" << endl;

	// step 3: sum
	for (int i = 0; i < rows; i++){
		sum[i] = ez[i * cols];
		for (int j = 1; j < cols; j++){
			sum[i] = sum[i] + ez[i * cols + j];
		}
	}

	// step 4: div
	ZKDiv(party, sum, t, rows);
	cout << "finish ZKDiv" << endl;
	
	// step 4: mult + trunc
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			y[i * cols + j] = t[i] * ez[i * cols + j];
		}
	}
	ZKpositiveTruncAny(party, y, y, rows * cols, SCALE);
	cout << "finish ZKpositiveTrunc" << endl;

	delete[] max;
	delete[] z;
	delete[] ez;
	delete[] sum;
	delete[] t;
}

void ZKLayerNorm(int party, IntFp *x, IntFp *y, IntFp *gamma, IntFp *beta, int rows, int cols)  // gamma: rows; beta: rows
{
	int64_t x_scale = floor((double)(1.0/cols) * (1ULL << SCALE));
    uint64_t cols_field = x_scale < 0 ? PR + x_scale : x_scale; 

	// step 1: compute mu + trunc
	IntFp *mu = new IntFp[rows];
	IntFp *sum = new IntFp[rows];
	for (int i = 0; i < rows; i++){
		sum[i] = x[i * cols];
		for (int j = 1; j < cols; j++){
			sum[i] = sum[i] + x[i * cols + j];
		}
		mu[i] = sum[i] * cols_field;
	}
	ZKgeneralTruncAny(party, mu, mu, rows, SCALE);

	// step 2: compute sigma
	IntFp *sigma = new IntFp[rows];
	IntFp *tmp = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		tmp[i * cols] = x[i * cols] + mu[i].negate();
		sum[i] = tmp[i * cols] * tmp[i * cols];
		for (int j = 1; j < cols; j++){
			tmp[i * cols + j] = x[i * cols + j] + mu[i].negate();
			sum[i] = sum[i] + (tmp[i * cols + j] * tmp[i * cols + j]);
		}
		sigma[i] = sum[i] * cols_field;
	}
	ZKpositiveTruncAny(party, sigma, sigma, rows, 2 * SCALE);

	for (int i = 0; i < rows; i++){    // //
		sigma[i] = sigma[i] + 1;
	}

	// step 3: compute t - sqrt
	IntFp *t = new IntFp[rows];
	ZKrSqrt(party, sigma, t, rows, 1);

	// step 4: compute z
	IntFp *z = new IntFp[rows * cols];
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			z[i * cols + j] = t[i] * tmp[i * cols + j];
		}
	}
	ZKgeneralTruncAny(party, z, z, rows * cols, SCALE);

	// step 5: compute y
	for (int i = 0; i < rows; i++){
		for (int j = 0; j < cols; j++){
			y[i * cols + j] = z[i * cols + j] * gamma[i] + beta[i];
		}
	}
	ZKgeneralTruncAny(party, y, y, rows * cols, SCALE);

	delete[] mu;
	delete[] sum;
	delete[] sigma;
	delete[] tmp;
	delete[] t;
	delete[] z;
}

