#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <emp-zk/ai/ai.h>
#include <emp-zk/ai/utils.h>
#include <emp-zk/ai/secure-utils.h>

#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int sz = 0;

const char* PARAMS_PATH = "test/ai/data/test.txt";

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


void test_affine_secure(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    init_verification();

    startComputation(party);

    IntFp* W_IntFp = new IntFp[sz+1];
    IntFp* x_IntFp = new IntFp[sz+1];

    // cleartext float
    float* W = new float[sz+1];
    float* x = new float[sz+1];

    if(party == ALICE){        
        read_next_elements(sz, W, 0, PARAMS_PATH);
        read_next_elements(sz, x, sz, PARAMS_PATH);
    }
    authenticate_over_field(sz, W, W_IntFp, party);
    authenticate_over_field(sz, x, x_IntFp, party);

    
    IntFp y = inner_product_bundle(sz, W_IntFp, x_IntFp, party);
    cout << y.reveal() << "\n";

    ZKgeneralTruncAny(party, &y, &y, 1, FXPSCALE);
    cout << y.reveal() << "\n";

    IntFp z(PR - 8.483 * (1 << FXPSCALE), ALICE);
    IntFp r = inner_product_bundle(1, &y, &z, party);
    ZKgeneralTruncAny(party, &r, &r, 1, FXPSCALE);

    cout << r.reveal() << "\n";

    bool cheated = finalize_zk_arith<BoolIO<NetIO>>();
    if(party == BOB && cheated){
        error("Inner product check failed!\n");
    }
}
    

void test_relu_secure(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    init_verification();
    startComputation(party);

    IntFp* lbs_IntFp = new IntFp[sz];
    IntFp* ubs_IntFp = new IntFp[sz];

    // cleartext float
    float* lbs = new float[sz];
    float* ubs = new float[sz];

    if(party == ALICE){        
        read_next_elements(sz, lbs, 0, PARAMS_PATH);
        read_next_elements(sz, ubs, sz, PARAMS_PATH);
    }
    authenticate_over_field(sz, lbs, lbs_IntFp, party);
    authenticate_over_field(sz, ubs, ubs_IntFp, party);

    
    auto constraints = relu_bundle(sz, lbs_IntFp, ubs_IntFp, party);

    cout << "Lower Constraints:\n";
    for(int i = 0; i < sz; i++){
        cout << format_EMP_IntFp(constraints.first[i].reveal(), 1) << " " << format_EMP_IntFp(constraints.first[i + sz].reveal(), 1) << "\n";
    }

    cout << "Upper Constraints:\n";
    for(int i = 0; i < sz; i++){
        cout << format_EMP_IntFp(constraints.second[i].reveal(), 1) << " " << format_EMP_IntFp(constraints.second[i + sz].reveal(), 1) << "\n";
    }

    bool cheated = finalize_zk_arith<BoolIO<NetIO>>();
    if(party == BOB && cheated){
        error("Inner product check failed!\n");
    }
}

void test_relu2_secure(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    init_verification();
    startComputation(party);

    IntFp* lbs_IntFp = new IntFp[sz];
    IntFp* ubs_IntFp = new IntFp[sz];

    // cleartext float
    float* lbs = new float[sz];
    float* ubs = new float[sz];

    if(party == ALICE){        
        read_next_elements(sz, lbs, 0, PARAMS_PATH);
        read_next_elements(sz, ubs, sz, PARAMS_PATH);
    }
    authenticate_over_field(sz, lbs, lbs_IntFp, party);
    authenticate_over_field(sz, ubs, ubs_IntFp, party);

    
    auto constraints = relu_bundle2(sz, lbs_IntFp, ubs_IntFp, party);

    cout << "Lower Constraints:\n";
    for(int i = 0; i < sz; i++){
        cout << format_EMP_IntFp(constraints.first[i].reveal(), 1) << " " << format_EMP_IntFp(constraints.first[i + sz].reveal(), 1) << "\n";
    }

    cout << "Upper Constraints:\n";
    for(int i = 0; i < sz; i++){
        cout << format_EMP_IntFp(constraints.second[i].reveal(), 1) << " " << format_EMP_IntFp(constraints.second[i + sz].reveal(), 1) << "\n";
    }

    bool cheated = finalize_zk_arith<BoolIO<NetIO>>();
    if(party == BOB && cheated){
        error("Inner product check failed!\n");
    }
}
    


int main(int argc, char** argv){
    parse_party_and_port(argv, &party, &port);
    BoolIO<NetIO> *ios[threads];
    for (int i = 0; i < threads; ++i)
        ios[i] = new BoolIO<NetIO>(
            new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
            party == ALICE);
            
    sz = atoi(argv[3]);
    FXPSCALE = atoi(argv[4]);
    // test_affine(ios, party);
    test_affine_secure(ios, party);
    test_relu_secure(ios, party);
    cout << "\n\n";
    test_relu2_secure(ios, party);
}