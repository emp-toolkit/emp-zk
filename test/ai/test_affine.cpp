#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <emp-zk/ai/ai.h>

#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int sz = 0;


void test_affine(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

	PRG prg;

    // cleartext float
    float* W = new float[sz * (sz + 1)];
    float* x = new float[sz];
    float* y = new float[sz];

    // cleartext fixpoint
    int64_t* W_int = new int64_t[sz*(sz+1)];
    int64_t* x_int = new int64_t[sz+1];
    int64_t* y_int = new int64_t[sz];

    // EMP fixpoint
    Integer* W_Integer = new Integer[sz*(sz+1)];
    Integer* x_Integer = new Integer[sz+1];
    Integer* y_Integer = new Integer[sz];
    Integer* y_res_Integer = new Integer[sz];

    // EMP Field
    IntFp* W_IntFp = new IntFp[sz*(sz+1)];
    IntFp* x_IntFp = new IntFp[sz+1];
    IntFp* y_IntFp = new IntFp[sz];

    for(int i = 0; i < sz; i++){
        for(int j = 0; j < sz+1; j++){
            int ia;
		    prg.random_data(&ia, 4);
		    W[i*(sz+1) + j] = (float)(ia) / 1000000000.0;
            W_int[i*(sz+1) + j] = W[i*(sz+1) + j] * (1 << FXPSCALE);
            W_Integer[i*(sz+1) + j] = Integer(FXPBW, W_int[i*(sz+1) + j] > 0 ? W_int[i*(sz+1) + j] : PR + W_int[i*(sz+1) + j], PUBLIC);
            W_IntFp[i*(sz+1) + j] = IntFp(W_Integer[i*(sz+1) + j].reveal<uint64_t>(), PUBLIC);
        }

        int ib;
		prg.random_data(&ib, 4);
        x[i] = (float)(ib) / 1000000000.0;
        x_int[i] = x[i] * (1 << FXPSCALE);
        x_Integer[i] = Integer(FXPBW, x_int[i] > 0 ? x_int[i] : PR + x_int[i], PUBLIC);
        x_IntFp[i] = IntFp(x_Integer[i].reveal<uint64_t>(), PUBLIC);
    }
    x[sz] = 1;
    x_int[sz] = x[sz] * (1 << FXPSCALE);
    x_Integer[sz] = Integer(FXPBW, x_int[sz], PUBLIC);
    x_IntFp[sz] = IntFp(x_Integer[sz].reveal<uint64_t>(), PUBLIC);

    cout << "AFFINE [FLOAT]  :\n";
    affine_layer(sz, sz, W, x, y);
    for(int i = 0; i < sz; i++){
        cout << y[i] << " ";
    }
    cout << "\n";

    cout << "AFFINE [FXP CLR]:\n";
    affine_layer(sz, sz, W_int, x_int, y_int);
    for(int i = 0; i < sz; i++){
        cout << (y_int[i]*1.0 / (1 << FXPSCALE))/(1 << FXPSCALE) << " ";
    }
    cout << "\n";

    cout << "AFFINE [FXP EMP]:\n";
    affine_layer(sz, sz, W_Integer, x_Integer, y_Integer);
    for(int i = 0; i < sz; i++){
        cout << format_EMP_Integer(y_Integer[i], 2) << " ";
    }
    cout << "\n";


    cout << "AFFINE [FLD EMP]:\n";
    affine_layer(sz, sz, W_IntFp, x_IntFp, y_IntFp);
    for(int i = 0; i < sz; i++){
        cout << format_EMP_IntFp(y_IntFp[i], 2) << " ";
    }
    cout << "\n";

    finalize_plain_prot();
}


int main(int argc, char** argv){
    parse_party_and_port(argv, &party, &port);
    BoolIO<NetIO> *ios[threads];
    for (int i = 0; i < threads; ++i)
        ios[i] = new BoolIO<NetIO>(
            new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
            party == ALICE);
            
    sz = atoi(argv[3]);
    test_affine(ios, party);
}