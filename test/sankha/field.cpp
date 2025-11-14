#include "emp-tool/emp-tool.h"
#include "emp-zk/emp-zk-arith/emp-zk-arith.h"
#include "emp-zk/extensions/floats.h"
#include <iostream>
using namespace emp;
using namespace std;

int port, party;
const int threads = 1;
int test_n = 1;


void relu(BoolIO<NetIO> *ios[threads], int party) {
    cout << "\n\n------ FIELD RELU ------\n";
    
    auto start = clock_start();
    setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
    auto timesetup = time_from(start);
    cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


    int sz = test_n;

    IntFp* x_Fp = new IntFp[sz];
    Integer* x_int = new Integer[sz];
    for(int i = 0; i < sz; i++){
        uint64_t x = 3 % PR;
        if(i & 1){
            x = PR - x;
        }
        
        x_Fp[i] = IntFp(x, PUBLIC);
    }


    start = clock_start();
    ios[0]->counter = 0;

    cout << x_Fp[0].reveal() << "\n";
    cout << x_Fp[1].reveal() << "\n";
    arith2bool<BoolIO<NetIO>>(x_int, x_Fp, sz);
    cout << x_int[0].reveal<string>() << "\n";
    cout << x_int[1].reveal<string>() << "\n";

    Integer zero = Integer(62, 0, PUBLIC);
    for(int i = 0; i < sz; i++){      
        // cout << x_int[i].reveal<uint64_t>() << " --> ";
        // x_int[i] = x_int[i].If(x_int[i] < (zero), zero);
        // cout << x_int[i].reveal<uint64_t>() << "\t";
    }
    cout << "\n";

    bool2arith<BoolIO<NetIO>>(x_Fp, x_int, sz);

    finalize_zk_bool<BoolIO<NetIO>>();
    finalize_zk_arith<BoolIO<NetIO>>();

    auto timeonline = time_from(start);
    cout << "Time: " << timeonline/sz << " us (" << sz << " operations) \n";
    cout << "Comm: " << ios[0]->counter/sz << " bytes (" << sz << " operations) \n";

    std::cout << "\a" << std::flush; 
}


void fix_vs_field(BoolIO<NetIO> *ios[threads], int party) {
    // setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
    // setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
    setup_plain_prot(false, "");

    float a = 3.14;
    Float a_float(a, ALICE);
    Integer a_int = FloatToInt62(a_float, 16);

    float b = 90.583;
    Float b_float(b, ALICE);
    Integer b_int = FloatToInt62(b_float, 16);
    
    cout << (a_int+b_int).reveal<uint64_t>() << "\n";
}

void relu_fix(BoolIO<NetIO> *ios[threads], int party) {
    float a = 3.141;
    Float a_float(a, PUBLIC);
    Integer a_int = FloatToInt62(a_float, 16);
    Integer PUBZERO(62, 0, PUBLIC);
    Bit c = a_int.geq(PUBZERO);
    cout << a_int.reveal<string>() << ":" << c.reveal<string>() << "\n";

    a = -a;
    a_float = Float(a, PUBLIC);
    a_int = FloatToInt62(a_float, 16);
    PUBZERO = Integer(62, 0, PUBLIC);
    c = a_int.geq(PUBZERO);
    cout << a_int.reveal<string>() << ":" << c.reveal<string>() << "\n";
}

void comp_fix(BoolIO<NetIO> *ios[threads], int party){
    setup_plain_prot(false, "");

    float a = 3.141, b = -4.442;
    Float a_float(a, PUBLIC);
    Float b_float(b, PUBLIC);

    Integer a_int = FloatToInt62(a_float, 16);
    Integer b_int = FloatToInt62(b_float, 16);

    cout << a_int.reveal<string>() << "\n";
    cout << b_int.reveal<string>() << "\n";


    Float zero(0, PUBLIC);
    cout << (a_float.less_equal(zero)).reveal<string>() << "\n";
    cout << (b_float.less_equal(zero)).reveal<string>() << "\n";
    // cout << (b_int >= zero).reveal<string>() << "\n";

    finalize_plain_prot();
}

int main(int argc, char **argv) {

    parse_party_and_port(argv, &party, &port);
    BoolIO<NetIO> *ios[threads];
    for (int i = 0; i < threads; ++i)
        ios[i] = new BoolIO<NetIO>(
            new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i),
            party == ALICE);
    
    if(argc > 3)  test_n = atoi(argv[3]);
        
    std::cout << std::endl
                << "------------ circuit zero-knowledge proof test ------------"
                << std::endl
                << std::endl;

    relu(ios, party);
    // relu(ios, party);
    // fix_vs_field(ios, party);
    // relu_fix(ios, party);
    // comp_fix(ios, party);

    for (int i = 0; i < threads; ++i) {
        delete ios[i]->io;
        delete ios[i];
    }
    return 0;
}
