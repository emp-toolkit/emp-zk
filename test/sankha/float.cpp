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
    cout << "\n\n------ FLOATING POINT RELU ------\n";
    
    auto start = clock_start();
    setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
    auto timesetup = time_from(start);
    cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


    int sz = test_n;

    IntFp* x_Fp = new IntFp[sz];
    Integer* x_int = new Integer[sz];
    for(int i = 0; i < sz; i++){
        uint64_t x = rand() % PR;
        if(i & 1){
            x = (PR - x) % PR;
        }
        
        x_Fp[i] = IntFp(x, ALICE);
    }

    start = clock_start();
    ios[0]->counter = 0;

    arith2bool<BoolIO<NetIO>>(x_int, x_Fp, sz);

    Float zero = Float(0, PUBLIC);
    for(int i = 0; i < sz; i++){
        Float x_float = Int62ToFloat(x_int[i], 16);
        
        x_float = x_float.If(x_float.less_equal(zero), zero);

        x_int[i] = FloatToInt62(x_float, 16);
    }

    bool2arith<BoolIO<NetIO>>(x_Fp, x_int, sz);

    finalize_zk_bool<BoolIO<NetIO>>();
    finalize_zk_arith<BoolIO<NetIO>>();

    auto timeonline = time_from(start);
    cout << "Time: " << timeonline/sz << " us (" << sz << " operations) \n";
    cout << "Comm: " << ios[0]->counter/sz << " bytes (" << sz << " operations) \n";

    std::cout << "\a" << std::flush; 
}


void relu_start_from_float(BoolIO<NetIO> *ios[threads], int party) {
    cout << "\n\n------ FLOATING POINT RELU ------\n";
    
    auto start = clock_start();
    setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
    auto timesetup = time_from(start);
    cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


    int sz = test_n;

    Float* x_float = new Float[sz];
    for(int i = 0; i < sz; i++){
        uint64_t a = rand() % PR;
        if(i & 1){
            a = (PR - a) % PR;
        }

        a = a / PR;
        
        x_float[i] = Float(a, ALICE);
    }

    start = clock_start();
    ios[0]->counter = 0;

    Float zero = Float(0, PUBLIC);
    for(int i = 0; i < sz; i++){        
        x_float[i] = x_float[i].If(x_float[i].less_equal(zero), zero);
    }

    finalize_zk_bool<BoolIO<NetIO>>();

    auto timeonline = time_from(start);
    cout << "Time: " << timeonline/sz << " us (" << sz << " operations) \n";
    cout << "Comm: " << ios[0]->counter/sz << " bytes (" << sz << " operations) \n";

    std::cout << "\a" << std::flush; 
}


void inner_product(BoolIO<NetIO> *ios[threads], int party) {
    cout << "\n\n------ FLOATING POINT INNER PRODUCT ------\n";
    
    auto start = clock_start();
    setup_zk_bool<BoolIO<NetIO>>(ios, threads, party);
    setup_zk_arith<BoolIO<NetIO>>(ios, threads, party, true);
    auto timesetup = time_from(start);
    cout << "\tsetup: " << timesetup/1e6 << " s; PARTY =" << party << " " << endl;


    int sz = test_n;

    IntFp* x_Fp = new IntFp[sz];
    Integer* x_int = new Integer[sz];
    for(int i = 0; i < sz; i++){
        uint64_t x = rand() % PR;
        if(i & 1){
            x = (PR - x) % PR;
        }
        
        x_Fp[i] = IntFp(x, ALICE);
    }

    start = clock_start();
    ios[0]->counter = 0;

    arith2bool<BoolIO<NetIO>>(x_int, x_Fp, sz);

    Float zero = Float(0, PUBLIC);
    for(int i = 0; i < sz; i++){
        Float x_float = Int62ToFloat(x_int[i], 16);
        
        x_float = x_float.If(x_float.less_equal(zero), zero);

        x_int[i] = FloatToInt62(x_float, 16);
    }

    bool2arith<BoolIO<NetIO>>(x_Fp, x_int, sz);

    finalize_zk_bool<BoolIO<NetIO>>();
    finalize_zk_arith<BoolIO<NetIO>>();

    auto timeonline = time_from(start);
    cout << "Time: " << timeonline/sz << " us (" << sz << " operations) \n";
    cout << "Comm: " << ios[0]->counter/sz << " bytes (" << sz << " operations) \n";

    std::cout << "\a" << std::flush; 
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
    relu_start_from_float(ios, party);

    for (int i = 0; i < threads; ++i) {
        delete ios[i]->io;
        delete ios[i];
    }
    return 0;
}
