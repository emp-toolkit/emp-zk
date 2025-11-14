#include "emp-tool/emp-tool.h"
#include <emp-zk/emp-zk.h>
#include <emp-zk/ai/ai.h>

#include <iostream>
#include <random>

using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int sz = 0;

void test_inner_product(){
    setup_plain_prot(false, "");

	PRG prg;

    // cleartext float
    float a[sz];
    float b[sz];
    for(int i = 0; i < sz; i++){
        int ia, ib;
		prg.random_data(&ia, 4);
		prg.random_data(&ib, 4);
		a[i] = (float)(ia) / 1000000000.0;
		b[i] = (float)(ib) / 1000000000.0;
    }
    cout << "Inner Product [FLOAT]: " << inner_product_emp(sz, a, b) << "\n";
    
    // cleartext fixpoint
    Float* a_float = new Float[sz];
    Float* b_float = new Float[sz];

    Integer* a_int = new Integer[sz];
    Integer* b_int = new Integer[sz];
    for(int i = 0; i < sz; i++){
        a_float[i] = Float(a[i], PUBLIC);
        b_float[i] = Float(b[i], PUBLIC);

        a_int[i] = FloatToInt62(a_float[i], FXPSCALE);
        b_int[i] = FloatToInt62(b_float[i], FXPSCALE);
    }
    
    Integer res = inner_product_emp(sz, a_int, b_int);
    Float res_fl = Int62ToFloat(res, 2*FXPSCALE);
    cout << "Inner Product [FXP]: " << res_fl.reveal<double>()  << "\n";

    finalize_plain_prot();
}


void test_inner_product_without_converters(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

	PRG prg;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1, 1);

    // cleartext float
    float a[sz];
    float b[sz];
    for(int i = 0; i < sz; i++){
        // int ia, ib;
		// prg.random_data(&ia, 4);
		// prg.random_data(&ib, 4);
		// a[i] = (float)(ia) / 1000000000.0;
		// b[i] = (float)(ib) / 1000000000.0;
        a[i] = dist(gen);
        b[i] = dist(gen);
    }
    cout << "Inner Product [FLOAT]  : " << inner_product_emp(sz, a, b) << "\n";
    
    // cleartext fixpoint
    int64_t* a_int = new int64_t[sz];
    int64_t* b_int = new int64_t[sz];
    for(int i = 0; i < sz; i++){
        a_int[i] = a[i] * (1 << FXPSCALE);
        b_int[i] = b[i] * (1 << FXPSCALE);
    }
    cout << "Inner Product [FXP CLR]: " << (inner_product_emp(sz, a_int, b_int)*1.0/(1 << FXPSCALE))/(1 << FXPSCALE) << "\n";

    Integer* a_Integer = new Integer[sz];
    Integer* b_Integer = new Integer[sz];
    for(int i = 0; i < sz; i++){
        a_Integer[i] = Integer(FXPBW, a_int[i] > 0 ? a_int[i] : PR + a_int[i], PUBLIC);
        b_Integer[i] = Integer(FXPBW, b_int[i] > 0 ? b_int[i] : PR + b_int[i], PUBLIC);
    }
    Integer res_ip_Integer = inner_product_emp(sz, a_Integer, b_Integer);
    cout << "Inner Product [FXP EMP]: " << format_EMP_Integer(res_ip_Integer, 2) << "\n";

    IntFp* a_IntFp = new IntFp[sz];
    IntFp* b_IntFp = new IntFp[sz];
    for(int i = 0; i < sz; i++){
        a_IntFp[i] = IntFp(a_Integer[i].reveal<uint64_t>(), PUBLIC);
        b_IntFp[i] = IntFp(b_Integer[i].reveal<uint64_t>(), PUBLIC);
    }

    IntFp res = inner_product_emp(sz, a_IntFp, b_IntFp);

    cout << "Inner Product [FLD EMP]: " << format_EMP_IntFp(res, 2) << "\n";

    finalize_plain_prot();
}


void test_relu_without_converters(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

	PRG prg;

    // cleartext float
    float a[sz];
    cout << "ReLU [FLOAT] : ";
    for(int i = 0; i < sz; i++){
        int ia, ib;
		prg.random_data(&ia, 4);
		a[i] = (float)(ia) / 1000000000.0;
        cout << (a[i] > 0 ? a[i] : 0) << " ";
    }
    cout << "\n";

    int64_t* a_int = new int64_t[sz];
    Integer* a_Integer = new Integer[sz];
    Integer* a_ReLU = new Integer[sz];
    Integer ZERO(FXPBW, 0, PUBLIC);
    cout << "ReLU [FXP]   : ";
    for(int i = 0; i < sz; i++){
        a_int[i] = a[i] * (1 << FXPSCALE);
        a_Integer[i] = Integer(FXPBW, a_int[i] > 0 ? a_int[i] : PR + a_int[i], PUBLIC);
        // a_Integer[i] = Integer(FXPBW, a_int[i], PUBLIC);
    
        a_ReLU[i] = a_Integer[i].If(!a_Integer[i].geq(ZERO), ZERO);
        cout << a_ReLU[i].reveal<uint64_t>()*1.0 / (1 << FXPSCALE) << " ";
    }
    cout << "\n";
}


void test_Integer(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    float a = -4.2921;
    int64_t a_int = a * (1 << FXPSCALE);

    cout << a_int << "\n";

    Integer a_Integer0(FXPBW, a_int);
    Integer a_Integer1(FXPBW, PR + a_int);

    cout << a_Integer0.reveal<string>() << "\n";
    cout << a_Integer1.reveal<string>() << "\n";

    IntFp a_IntFp0(a_Integer0.reveal<uint64_t>(), a_int);
    IntFp a_IntFp1(a_Integer1.reveal<uint64_t>(), a_int);
    cout << a_IntFp0.reveal() << "\n";
    cout << a_IntFp1.reveal() << "\n";
}



void test_double_mult(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    float* a_float = new float[6];
    int64_t* a_int = new int64_t[6];
    Integer* a_Integer = new Integer[6];
    IntFp* a_IntFp = new IntFp[6];

    a_float[0] = 3.141;
    a_float[1] = 2.712;
    a_float[2] = a_float[0] * a_float[1];
    cout << a_float[2] << "\n";

    for(int i = 0; i < 2; i++){
        a_int[i] = a_float[i] * (1 << FXPSCALE);
        a_Integer[i] = Integer(FXPBW, a_int[i] > 0 ? a_int[i] : PR + a_int[i]);
        a_IntFp[i] = IntFp(a_Integer[i].reveal<uint64_t>());
    }
    
    a_int[2] = a_int[0] * a_int[1];
    a_Integer[2] = (a_Integer[0] * a_Integer[1]);
    a_IntFp[2] = a_IntFp[0] * a_IntFp[1];

    cout << a_Integer[2].reveal<uint64_t>() << "\n";
    cout << format_EMP_Integer(a_Integer[2]) << "\n";
}   


void test_Integer_signed(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");

    float a_float = -3.141;
    int64_t a_int = a_float * (1 << FXPSCALE);
    cout << a_int << "\n";

    Integer a_Integer(FXPBW, a_int);
    cout << REVERSE(a_Integer.reveal<string>()) << "\n";

    Integer a_Integer1(-a_Integer);
    cout << REVERSE(a_Integer1.reveal<string>()) << "\n";

}


void test_IntFp_signed(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);
    IntFp* a_IntFp = new IntFp[4];

    for(int i = 0; i < 2; i++){
        a_IntFp[i] = IntFp(PR - 3.141 * (1 << FXPSCALE), PUBLIC);
    }

    a_IntFp[2] = a_IntFp[0] * a_IntFp[1];
    cout << a_IntFp[2].reveal() << "\n";

    a_IntFp[3] = a_IntFp[2] * a_IntFp[1];

    a_IntFp[3] = IntFp((PR - 1)/2, PUBLIC);
    cout << a_IntFp[3].reveal() << "\n";

    Integer a_Integer(FXPBW, a_IntFp[3].reveal());
    cout << REVERSE(a_Integer.reveal<string>()) << "\n";

}


void test_Integer_normalization(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");

    float a_float = -3.141;
    int64_t a_int = a_float * (1 << FXPSCALE);
    Integer a_Integer(FXPBW, a_int);

    a_Integer = a_Integer * Integer(FXPBW, -13.141*(1 << FXPSCALE));
    cout << (a_Integer.reveal<uint64_t>()) << "\n";

    a_Integer = normalize(a_Integer);
    cout << (a_Integer.reveal<uint64_t>()) << "\n";
}


int main(int argc, char** argv){
    parse_party_and_port(argv, &party, &port);
    BoolIO<NetIO> *ios[threads];
    for (int i = 0; i < threads; ++i)
        ios[i] = new BoolIO<NetIO>(
            new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
            party == ALICE);
            
    sz = atoi(argv[3]);
    // test_inner_product();
    // test_inner_product_without_converters(ios, party);
    // test_relu();
    // test_relu_without_converters(ios, party);
    // test_Integer(ios, party);
    // test_double_mult(ios, party);
    // test_IntFp_signed(ios, party);
    test_Integer_normalization(ios, party);
}