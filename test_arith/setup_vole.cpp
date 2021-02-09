#include "vole-arith/emp-vole.h"
#include "emp-tool/emp-tool.h"
#if defined(__linux__)
	#include <sys/time.h>
	#include <sys/resource.h>
#elif defined(__APPLE__)
	#include <unistd.h>
	#include <sys/resource.h>
	#include <mach/mach.h>
#endif

using namespace emp;
using namespace std;

int party, port;
const int threads = 0;

void test_vole_triple(NetIO *ios[threads+1], int party) {
	VoleTriple<threads> vtriple(party, ios);


	__uint128_t Delta = (__uint128_t)0;
	auto start = clock_start();
	if(party == ALICE) {
		PRG prg;
		prg.random_data(&Delta, sizeof(__uint128_t));
		Delta = Delta & ((__uint128_t)0xFFFFFFFFFFFFFFFFLL);
		Delta = mod(Delta, pr);
		vtriple.setup(Delta);
		//vtriple.check_triple(Delta, vtriple.pre_yz, vtriple.n_pre);
	} else {
		vtriple.setup();
		//vtriple.check_triple(0, vtriple.pre_yz, vtriple.n_pre);
	}

	std::cout << "setup\t" << time_from(start) << " us" << std::endl;

#if defined(__linux__)
	struct rusage rusage;
	if (!getrusage(RUSAGE_SELF, &rusage))
		std::cout << "[Linux]Peak resident set size: " << (size_t)rusage.ru_maxrss << std::endl;
	else std::cout << "[Linux]Query RSS failed" << std::endl;
#elif defined(__APPLE__)
	struct mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
		std::cout << "[Mac]Peak resident set size: " << (size_t)info.resident_size_max << std::endl;
	else std::cout << "[Mac]Query RSS failed" << std::endl;
#endif

	// communication
	uint64_t comm = 0;
	for(int i = 0; i < threads+1; ++i) {
		comm += ios[i]->counter;
	}
	std::cout << "communication: " << comm << std::endl;
}

int main(int argc, char** argv) {
	parse_party_and_port(argv, &party, &port);
	NetIO* ios[threads+1];
	for(int i = 0; i < threads+1; ++i)
		ios[i] = new NetIO(party == ALICE?nullptr:"127.0.0.1",port+i);

	std::cout << std::endl << "------------ VOLE field ------------" << std::endl << std::endl;;

	test_vole_triple(ios, party);

	for(int i = 0; i < threads+1; ++i)
		delete ios[i];
	return 0;
}
