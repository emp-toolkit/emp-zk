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


uint64_t inv(uint64_t a, uint64_t p) {
    int64_t t = 0,   newt = 1;
    int64_t r = p,   newr = a;

    while (newr != 0) {
        uint64_t q = r / newr;
        int64_t tmp;

        tmp = newt; 
        newt = t - q * newt; 
        t = tmp;
        
        tmp = newr; 
        newr = r - q * newr; 
        r = tmp;
    }

    if (r > 1) return 0;        // not invertible
    if (t < 0) t += p;

    return t;
}

void fixed_point_divide(IntFp x, IntFp y){
    Integer x_Integer(FXPBW, x.reveal(), PUBLIC);
    Integer y_Integer(FXPBW, y.reveal(), PUBLIC);
    x_Integer = Integer(FXPBW, x_Integer.reveal<uint64_t>() << FXPSCALE, PUBLIC);

    cout << x_Integer.reveal<uint64_t>() << "\n";
    cout << y_Integer.reveal<uint64_t>() << "\n";

    y_Integer = x_Integer / y_Integer;
    cout << y_Integer.reveal<uint64_t>() << "\n";
}


void test_divide(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    float a_float = 5.3928;
    int64_t a_int = a_float * (1 << FXPSCALE);
    IntFp a_IntFp(a_int > 0 ? a_int : a_int + PR, PUBLIC);

    float b_float = -2.684;
    int64_t b_int = b_float * (1 << FXPSCALE);
    IntFp b_IntFp(a_int > 0 ? b_int : b_int + PR, PUBLIC);

    float c_float = 1.333;
    int64_t c_int = c_float * (1 << FXPSCALE);
    IntFp c_IntFp(c_int > 0 ? c_int : c_int + PR, PUBLIC);

    // fixed_point_divide(a_IntFp, b_IntFp);


    cout << a_float/(a_float - b_float) << "\n";
    cout << (a_float*b_float)/(a_float - b_float) << "\n";

    c_IntFp = divide(a_IntFp, subtract(a_IntFp, b_IntFp));
    cout << format_EMP_IntFp(c_IntFp, 1) << "\n";


    IntFp d_IntFp = a_IntFp*b_IntFp;
    normalize(1, &d_IntFp, &d_IntFp);
    c_IntFp = divide(d_IntFp, subtract(a_IntFp, b_IntFp));
    cout << format_EMP_IntFp(c_IntFp, 1) << "\n";
}


void test_inverse(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party);

    float a_float = 30;
    int64_t a_int = a_float * (1 << FXPSCALE);
    IntFp a_IntFp(a_int > 0 ? a_int : a_int + PR, PUBLIC);


    float b_float = 5;
    int64_t b_int = b_float * (1 << FXPSCALE);
    IntFp b_IntFp(b_int > 0 ? b_int : b_int + PR, PUBLIC);


    IntFp c_IntFp(0, PUBLIC);
    c_IntFp = IntFp(inv(b_IntFp.reveal(), PR));

    cout << c_IntFp.reveal() << "\n";

    c_IntFp = a_IntFp * c_IntFp;

    // fixed_point_divide(a_IntFp, b_IntFp);

    cout << a_IntFp.reveal() << "\n";
    cout << format_EMP_IntFp(c_IntFp, 0) << "\n";
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
    // test_divide(ios, party);
    test_inverse(ios, party);
}