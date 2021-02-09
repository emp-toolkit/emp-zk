#ifndef EMP_ZK_BOOL_H__
#define EMP_ZK_BOOL_H__
#include "emp-zk-bool/zk_prover.h"
#include "emp-zk-bool/zk_verifier.h"
#include "emp-zk-bool/zk_bool_circuit_exec.h"
#include "emp-zk-bool/ostriple.h"
#include "emp-zk-bool/polynomial.h"

template<typename IO>
inline void setup_zk_bool(IO** ios, int threads, int party) {
	if(party == ALICE) {
		ZKBoolCircExecPrv<IO> * t = new ZKBoolCircExecPrv<IO>();
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new ZKProver<IO>(ios, threads, t);
		((ZKProver<IO>*)(ProtocolExecution::prot_exec))->ostriple->init_polyproof();
	} else {
		ZKBoolCircExecVer<IO> * t = new ZKBoolCircExecVer<IO>();
		CircuitExecution::circ_exec = t;
		ProtocolExecution::prot_exec = new ZKVerifier<IO>(ios, threads, t);
		((ZKVerifier<IO>*)(ProtocolExecution::prot_exec))->ostriple->init_polyproof();
	}
}

template<typename IO>
inline block get_bool_delta(int party) {
	if(party == BOB)
		return ((ZKVerifier<IO>*)(ProtocolExecution::prot_exec))->ostriple->delta;
	else zero_block;
}

template<typename IO>
inline void sync_zk_bool() {
	((ZKBoolCircExec<IO>*)CircuitExecution::circ_exec)->sync();
}

template<typename IO>
inline bool finalize_zk_bool(int party) {
	bool res = ((ZKBoolCircExec<IO>*)CircuitExecution::circ_exec)->ostriple->check_cheat();
	delete CircuitExecution::circ_exec;
	delete ProtocolExecution::prot_exec;
	return res;
}

template<typename IO>
inline void zkp_poly_deg2(Bit *x, Bit *y, bool *coeff, int len) {
	PolyProof<IO>::polyproof->zkp_poly_deg2((block*)x, (block*)y, coeff, len);
}

template<typename IO>
inline void zkp_inner_prdt(Bit *x, Bit *y, bool constant, int len) {
	PolyProof<IO>::polyproof->zkp_inner_prdt((block*)x, (block*)y, constant, len);
}
#endif
