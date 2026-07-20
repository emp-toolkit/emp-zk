# Agent guide for emp-zk

Entry point for AI coding agents working on this repository. Read this
file first.

## Project at a glance

emp-zk is the zero-knowledge layer of the EMP toolkit, on top of
emp-tool and emp-ot. It implements the VOLE-based ZK line of
[Wolverine](https://eprint.iacr.org/2020/925) /
[Quicksilver](https://eprint.iacr.org/2021/076) /
[Mystique](https://eprint.iacr.org/2021/730), in three parts:

- **`emp-zk-bool`** — Boolean-circuit ZK. Fully modern: a native
  emp-tool `BooleanContext` (`ZKBoolContext`) driven by an explicit
  `ZKBoolSession` handle. There is **no global backend**; every gadget
  receives the session explicitly. Circuit values are `ZKBit` (`Bit_T`)
  and the runtime-width `ZKUInt` / `ZKInt` (`UInt_T` / `Int_T<…, 0>` over
  `ZKBoolContext`; fixed-width via `Int_T<…, N>`). The correlated-OT engine
  is emp-ot's `SilentFerret` with sized prepay (`expected_cots`). The session models
  emp-tool's `Session` / `DirectSession` / `SessionIO` concepts
  (`static_assert`s in [`zk_session.h`](emp-zk/emp-zk-bool/zk_session.h)).
- **`emp-zk-arith`** — a special-purpose algebraic-ZK library over
  authenticated field elements (`IntFp`, prime field p = 2⁶¹ − 1): private
  inputs, multiplication, inner-product / polynomial-relation gadgets, and
  the edabit bool↔arith conversion bridge (matrix multiplication and SIS
  are tested example constructions under `test/arith/`, not library
  gadgets). It still uses a global
  `ZKFpExec::zk_exec` singleton plus `setup_zk_arith` /
  `finalize_zk_arith` free functions. A planned de-singletonization —
  the arithmetic analog of the Boolean session migration — is designed
  in [docs/arith-modernization.md](docs/arith-modernization.md).
- **RAM / ROM / set-membership** ZK gadgets (`ZKRam` / `ZKROM` /
  `ZKSet`, plus a permutation proof) built on the Boolean session.

## Building and running tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

emp-tool / emp-ot must be discoverable. If they are sibling source
trees rather than installed, point CMake at their build directories:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -Demp-tool_DIR=/path/to/emp-tool/build \
    -Demp-ot_DIR=/path/to/emp-ot/build
```

Every test is two-party — a prover and a verifier running concurrently
on localhost. The `./run` wrapper does the launch:

```bash
./run ./build/test_bool_example
./run ./build/test_arith_zk_proof
```

`ctest` registers two-party tests via `add_test_case_with_run`. To
rebuild + re-run one in isolation:

```bash
cmake --build build -j --target test_arith_sis
./run ./build/test_arith_sis
```

A separate `build-debug/` (`-DCMAKE_BUILD_TYPE=Debug`) is the
convention for a debug tree.

## Top-level rules (apply to all work)

- **Boolean side: no global backend.** A proof is driven through a
  `ZKBoolSession` that owns the engine and is the I/O boundary; gadgets
  take the session (or its context) explicitly and follow the
  session-first signature `zkp_*(ZKBoolSession&, …)`. Don't reintroduce
  a global `CircuitExecution` / `ProtocolExecution`-style singleton —
  that was the v0.3.x design and it's gone.

- **Arith side: it's a gadget library, not an incomplete migration.**
  The `ZKFpExec` engine + `IntFp` value + specialized gadgets is the
  intended shape for special-op arithmetic ZK. Its global singleton is
  slated for removal under the plan in
  [docs/arith-modernization.md](docs/arith-modernization.md) (an
  emp-tool `ArithmeticContext` concept + an emp-zk `ZKFpSession`); don't
  do a partial ad-hoc de-singletonization outside that plan.

- **Contracts use `expecting()` / `error()` (always-on).** Check a
  precondition or invariant with `expecting(cond, msg)`; fail an
  unreachable / bad-state branch with `error(msg)`. Both abort in every
  build flavor — don't use raw `assert()` (compiled out under `NDEBUG`)
  for a protocol contract.

- **Two-party test discipline.** Tests use `NetIO::listen` / `connect`
  via the `./run` wrapper. If a test needs more than one channel between
  the parties, take siblings from one anchor with `NetIO::make_sibling`
  rather than opening a second same-port connection (avoids reconnect
  races).

- **Correctness gate: cleartext-context replay.** `test_context_zk` and
  `sha256` run a circuit in ZK and validate it bit-for-bit against a
  `ClearCtx` replay — the cross-context check that catches drift against
  emp-tool's value/context API. The `Session` / `SessionIO`
  `static_assert`s are compile-time drift guards. (There is no
  wire-trace baseline gate like emp-ot's `trace_hash` yet; it's a
  roadmap item, see docs/arith-modernization.md.)

- **Comments describe current code only.** No "used to" / "formerly" /
  "the old …" narration of removed approaches. Buffer-length and count
  parameters use `int64_t`, matching emp-tool / emp-ot — except the
  width-agnostic raw-bit session boundary (`input_bits` / `reveal_bits`),
  which uses `size_t`.

- **No perf numbers in comments.** Benchmark deltas belong in commit
  messages, not source — they rot.

- **Commit policy.** Only commit when explicitly asked. When asked,
  surface the drafted message first and wait for a go-ahead; use
  `git commit -F <file>` for multiline messages (heredocs corrupt
  apostrophes). Don't push without an explicit instruction.
