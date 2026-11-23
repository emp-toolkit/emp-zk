// ZK SHA-256: replay the shipped sha256_256 builtin (a full 256-bit message hash)
// over the ZK boolean context and validate the result bit-for-bit against a
// ClearCtx replay of the same .empbc — the IR-replay path exercised in zero
// knowledge. C++20.

#include "emp-tool/emp-tool.h"
#include "emp-tool/ir/context/clear.h"
#include "emp-tool/ir/builtins.h"
#include "emp-tool/ir/execute.h"
#include <emp-zk/emp-zk.h>
#include <cstdio>
#include <memory>
#include <span>
#include <vector>

using namespace emp;

int party;

static std::vector<ZKWire> feed_wires(ZKBoolSession &sess, int owner,
                                      const std::vector<uint8_t> &bits) {
  const int n = (int)bits.size();
  auto b = std::make_unique<bool[]>((size_t)n);
  for (int i = 0; i < n; ++i) b[(size_t)i] = bits[(size_t)i] != 0;
  return sess.input_bits(owner, b.get(), (size_t)n);
}
static std::vector<uint8_t> reveal_wires(ZKBoolSession &sess, int recipient,
                                         const std::vector<ZKWire> &w) {
  const int n = (int)w.size();
  auto b = std::make_unique<bool[]>((size_t)n);
  sess.reveal_bits(b.get(), recipient, w.data(), (size_t)n);
  std::vector<uint8_t> out((size_t)n);
  for (int i = 0; i < n; ++i) out[(size_t)i] = b[(size_t)i] ? 1 : 0;
  return out;
}

int main(int argc, char **argv) {
  party = parse_party(argv);
  auto netio = (party == ALICE) ? NetIO::listen(peer_port()) : NetIO::connect(peer_ip(), peer_port());
  BoolIO io(netio.get(), party == ALICE);

  ZKBoolSession sess(&io, party);
  const circuit::BooleanProgram &prog = circuit::builtin_circuit("sha256_256");
  const int nin = (int)prog.num_inputs;

  // Deterministic test message (both parties agree; ALICE owns the witness).
  std::vector<uint8_t> wit((size_t)nin);
  for (int i = 0; i < nin; ++i)
    wit[(size_t)i] = (uint8_t)(((uint32_t)i * 0x9e3779b9u) >> 19) & 1u;

  auto zin = feed_wires(sess, ALICE, wit);
  auto zout = execute_program(sess.ctx(), prog,
                              std::span<const ZKWire>(zin.data(), zin.size()));
  auto zdig = reveal_wires(sess, PUBLIC, zout);

  ClearCtx cctx;
  auto cout = execute_program(cctx, prog,
                              std::span<const uint8_t>(wit.data(), (size_t)nin));
  for (size_t i = 0; i < cout.size(); ++i)
    expecting((zdig[i] & 1) == (cout[i] & 1),
              "sha256: the ZK output differs from the cleartext replay");

  sess.finalize();
  if (party == ALICE)
    printf("ZK sha256_256 replay (%zu output bits) — PASS\n", cout.size());

  return 0;
}
