#include "emp-tool/emp-tool.h"
#include "emp-ot/emp-ot.h"
#include "emp-zk/emp-zk.h"
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
const int threads = 4;

void check_triple(const block delta,
                  const block *x,
                  const block *y,
                  int size,
                  NetIO *io) {
  if (party == BOB) {
    io->send_data(&delta, sizeof(block));
    io->send_data(y, size * sizeof(block));
    io->flush();
  } else {
    block delta_;
    block *k = new block[size];
    io->recv_data(&delta_, sizeof(block));
    io->recv_data(k, size * sizeof(block));
    for (int i = 0; i < size; ++i) {
      block tmp;
      gfmul(delta_, x[i], &tmp);
      tmp = tmp ^ k[i];
      if ((memcmp(&tmp, &y[i], sizeof(block))) != 0) {
        std::cout << "triple error at index: " << i << std::endl;
        abort();
      }
    }
    delete[] k;
  }
}


void test_vole_triple(
  NetIO **ios, 
  BoolIO<NetIO> **ios_bool,
  int party) {
  // instantiate OT
  FerretCOT<BoolIO<NetIO>> ferretcot(
    3 - party, threads, ios_bool, true);

  // instantiate F2K VOLE
  SVoleF2k<BoolIO<NetIO>> vtriple(
    party, threads, ios_bool, &ferretcot);

  PRG prg;
  block Delta = ferretcot.Delta;
  auto start = clock_start();
  vtriple.setup(Delta);
  std::cout << "setup " << time_from(start) / 1000 << " ms" << std::endl;
  check_triple(Delta, vtriple.pre_x, vtriple.pre_yz, vtriple.param.n_pre, ios[0]);

  std::size_t triple_need = vtriple.ot_limit;
  std::size_t buf_sz = vtriple.param.n;
  block *buf_x = new block[buf_sz];
  block *buf_yz = new block[buf_sz];
  for(int i = 0; i < 16; ++i) {
    start = clock_start();
    vtriple.extend_inplace(buf_x, buf_yz, buf_sz);
    std::cout << "extend " << time_from(start) / 1000 << " ms" << std::endl;
    check_triple(Delta, buf_x, buf_yz, triple_need, ios[0]);
  }
  delete[] buf_x;
  delete[] buf_yz;

#if defined(__linux__)
  struct rusage rusage;
  if (!getrusage(RUSAGE_SELF, &rusage))
    std::cout << "[Linux]Peak resident set size: " << (size_t)rusage.ru_maxrss
              << std::endl;
  else
    std::cout << "[Linux]Query RSS failed" << std::endl;
#elif defined(__APPLE__)
  struct mach_task_basic_info info;
  mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info,
                &count) == KERN_SUCCESS)
    std::cout << "[Mac]Peak resident set size: "
              << (size_t)info.resident_size_max << std::endl;
  else
    std::cout << "[Mac]Query RSS failed" << std::endl;
#endif
}

int main(int argc, char **argv) {
  parse_party_and_port(argv, &party, &port);

  NetIO *ios[threads];
  for (int i = 0; i < threads; ++i)
    ios[i] = new NetIO(party == ALICE ? nullptr : "127.0.0.1", port + i);


  BoolIO<NetIO> *ios_bool[threads];
  for (int i = 0; i < threads; ++i)
    ios_bool[i] = new BoolIO<NetIO>(
        ios[i], party == ALICE);


  std::cout << std::endl
            << "------------ VOLE f2k ------------" << std::endl
            << std::endl;
  ;

  test_vole_triple(ios, ios_bool, party);

  for (int i = 0; i < threads; ++i) {
    delete ios[i];
    delete ios_bool[i];
  }
  return 0;
}
