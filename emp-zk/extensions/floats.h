#ifndef __EMP_ZK_FLOAT_
#define __EMP_ZK_FLOAT_

#include "emp-tool/emp-tool.h"
#include <iostream>
using namespace emp;
using namespace std;

inline Float Int32ToFloat(const Integer &input) {
	const Integer zero(32, 0, PUBLIC);
	const Integer one(32, 1, PUBLIC);
	const Integer maxInt(32, 1 << 24, PUBLIC); // 2^24
	const Integer minInt = Integer(32, -1 * (1 << 24), PUBLIC); // -2^24
	const Integer twentyThree(32, 23, PUBLIC);
	Float output(0.0, PUBLIC);
	Bit signBit = input.bits[31];
	Integer unsignedInput = input.abs();
	Integer firstOneIdx = Integer(32, 31, PUBLIC) - unsignedInput.leading_zeros().resize(32);
	Bit leftShift = firstOneIdx >= twentyThree;
	Integer shiftOffset = If(leftShift, firstOneIdx - twentyThree, twentyThree - firstOneIdx);
	Integer shifted = If(leftShift, unsignedInput >> shiftOffset, unsignedInput << shiftOffset);
	// exponent is biased by 127
	Integer exponent = firstOneIdx + Integer(32, 127, PUBLIC);
	// move exp to the right place in final output
	exponent = exponent << 23;
	Integer coefficient = shifted;
	// clear leading 1 (bit #23) (it will implicitly be there but not stored)
	coefficient.bits[23] = Bit(false, PUBLIC);
	// bitwise OR the sign bit | exp | coeff
	Integer outputInt(32, 0, PUBLIC);
	outputInt.bits[31] = signBit; // bit 31 is sign bit
	outputInt =  coefficient | exponent | outputInt;
	memcpy(&(output.value[0]), &(outputInt.bits[0]), 32 * sizeof(Bit));
	// cover the corner cases
	output = If(input == zero, Float(0.0, PUBLIC), output);
	output = If(input < minInt, Float(INT_MIN, PUBLIC), output);
	output = If(input > maxInt, Float(INT_MAX, PUBLIC), output);
	return output;
}

inline Integer FloatToInt62(Float input, int s) {
	Integer fraction(25, 0, PUBLIC);
	memcpy(fraction.bits.data(), input.value.data(), 23*sizeof(block));
	fraction[23] = Bit(true, PUBLIC);
	fraction.resize(61, false);

	Integer exp(8, 0, PUBLIC);
	memcpy(exp.bits.data(), input.value.data()+23, 8*sizeof(block));
	exp = exp - Integer(8, 127+23-s, PUBLIC);
	Integer negexp = -exp;
	fraction = If(!exp[7], fraction << exp, fraction >> negexp);
	fraction = If(negexp>=Integer(8, 61, PUBLIC), Integer(61, 0, PUBLIC), fraction);
	fraction = If(input[31], -fraction, fraction);
	fraction.resize(62, false);
	return fraction;
}

inline Float Int62ToFloat(Integer input, int s) {
	input = If(input > Integer(62, (1ULL<<60) - 1, PUBLIC), input - Integer(62, (1ULL<<61) - 1, PUBLIC),input);
	input.bits.pop_back();
	assert(input.size() == 61);
	const Integer twentyThree(8, 23, PUBLIC);

	Float output(0.0, PUBLIC);
	Bit signBit = input.bits[60];
	Integer unsignedInput = input.abs();
	//	for(int i = 0; i < 61; ++i)cout << unsignedInput[i].reveal(PUBLIC);cout <<endl;

	Integer firstOneIdx = Integer(8, 60, PUBLIC) - unsignedInput.leading_zeros().resize(8);
	Bit leftShift = firstOneIdx >= twentyThree;
	Integer shiftOffset = If(leftShift, firstOneIdx - twentyThree, twentyThree - firstOneIdx);
	Integer shifted = If(leftShift, unsignedInput >> shiftOffset, unsignedInput << shiftOffset);
	// exponent is biased by 127
	Integer exponent = firstOneIdx + Integer(8, 127 - s, PUBLIC);

	output.value[31] = signBit;
	memcpy(output.value.data()+23, exponent.bits.data(), 8*sizeof(block));
	memcpy(output.value.data(), shifted.bits.data(), 23*sizeof(block));
	return output;
}

#endif //__EMP_ZK_FLOAT_