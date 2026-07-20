# EMP-zk
![build](https://github.com/emp-toolkit/emp-zk/workflows/build/badge.svg)
[![CodeQL](https://github.com/emp-toolkit/emp-zk/actions/workflows/codeql.yml/badge.svg)](https://github.com/emp-toolkit/emp-zk/actions/workflows/codeql.yml)

<img src="https://raw.githubusercontent.com/emp-toolkit/emp-readme/main/art/logo-full.jpg" width=300px/>

> **Which version do I want?**
>
> - **Existing projects pinned to the previous line: stay on `v0.3.x`**
>   (the `v0.3.x` branch — there is no `0.3.0` tag). It is maintained separately
>   and receives backported fixes.
> - **New projects, or willing to migrate: use `v1.0.0-alpha.1`** (or track
>   `main`), which pairs with emp-tool and emp-ot `v1.0.0-alpha.1`. This is a
>   pre-1.0 alpha — headers and names may change between alpha tags and before
>   the final `1.0.0`, so pin to a specific tag and pin the paired emp-tool /
>   emp-ot tags. CMake package metadata is numeric `1.0.0` (a `project(VERSION)`
>   cannot carry a prerelease suffix); the alpha status lives in the git tag.

## Protocols

This repository implements fast, communication-efficient zero-knowledge proofs
for Boolean and arithmetic circuits and for polynomial relations, following
[Wolverine](https://eprint.iacr.org/2020/925),
[Quicksilver](https://eprint.iacr.org/2021/076), and
[Mystique](https://eprint.iacr.org/2021/730). It comprises:

- **`emp-zk-bool`** — Boolean-circuit ZK on a native emp-tool `BooleanContext`
  (`ZKBoolContext`) driven by an explicit `ZKBoolSession` handle: no global
  backend, gadgets receive the session explicitly, and circuit values are
  `ZKBit` (`Bit_T`) and the runtime-width `ZKUInt` / `ZKInt`
  (`UInt_T` / `Int_T<…, 0>` over `ZKBoolContext`); fixed-width integers are
  available as `Int_T<…, N>`. The correlated-OT engine is emp-ot's
  `SilentFerret` with sized prepay.
- **`emp-zk-arith`** — a special-purpose algebraic-ZK library over
  authenticated field elements (`IntFp`, prime field p = 2⁶¹ − 1): private
  inputs, multiplication, and inner-product / polynomial-relation gadgets, plus
  an edabit bool↔arith conversion bridge. Matrix multiplication and SIS are
  included as tested example constructions (see `test/arith/`).
- **RAM / ROM / set-membership** ZK gadgets (`ZKRam` / `ZKROM` / `ZKSet`, plus a
  permutation proof) built on the Boolean session.

## Requirements

- CMake ≥ 3.21 (emp-tool needs ≥ 3.25)
- A C++20 compiler
- OpenSSL ≥ 3.0
- emp-tool and emp-ot at the paired tag (`v1.0.0-alpha.1`)

## Build and install

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build      # respects CMAKE_INSTALL_PREFIX
```

If emp-tool / emp-ot are in sibling source trees rather than installed, point
CMake at their build directories:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -Demp-tool_DIR=/path/to/emp-tool/build \
    -Demp-ot_DIR=/path/to/emp-ot/build
cmake --build build -j
```

## Consuming from another CMake project

After `cmake --install build`:

```cmake
find_package(emp-zk 1.0 REQUIRED)
target_link_libraries(my-app PRIVATE emp-zk::emp-zk)
```

The `emp-zk::emp-zk` target transitively pulls in `emp-ot::emp-ot` and
`emp-tool::emp-tool`.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Tests under `test/bool/`, `test/arith/`, and `test/ram-zk/` exercise each
module: Boolean and arithmetic ZK, polynomial and inner-product proofs, a
SHA-256 circuit, edabit bool↔arith conversion, and RAM / ROM / set-membership
ZK. Two-party tests are driven by the top-level `./run` wrapper, which spawns
party 1 then party 2 on localhost.

For a two-machine run, set the port with `EMP_PORT` and point party 2 at
party 1 with `EMP_PEER_IP` (only the party id is positional):

```bash
# host A (party 1)
EMP_PORT=12345 ./build/test_bool_example 1
# host B (party 2)
EMP_PORT=12345 EMP_PEER_IP=<host-A-address> ./build/test_bool_example 2
```

## Security

Research software; there has been no independent security audit. Please report
vulnerabilities by email to Xiao Wang (wangxiao1254@gmail.com).

- **Arithmetic soundness is field-bounded, not 128-bit.** `emp-zk-arith` works
  over the prime field p = 2⁶¹ − 1, so its single-field consistency checks have
  soundness error on the order of 2⁻⁶¹. Boolean ZK operates at the 128-bit
  level.
- **Malicious mode is selective-abort, with provisional output.** A cheating
  prover is caught at the session's closing checks (`ZKBoolSession::finalize()`
  on the Boolean side; the arithmetic side settles its checks at teardown), and
  revealed values are provisional until finalization succeeds — do not act on a
  revealed value across a trust boundary before that point.
- There is no systematic constant-time guarantee.

## Performance

The tables below are historical numbers from the **v0.3.x line** (a
multi-threaded engine), measured on two AWS EC2 m5.2xlarge servers with a
throttled network, in million gates per second. The current `main` engine is
single-threaded and has not been re-benchmarked, so treat these as context,
not a benchmark of the alpha.

### Boolean circuits

|Threads|10 Mbps|20 Mbps|30 Mbps|50 Mbps|Localhost|
|-------|-------|-------|-------|-------|---------|
|1|5.1|7.8|8.6|8.6|8.6|
|2|6|10|12.9|14.3|13.6|
|3|6.3|10.9|14.5|17.3|18|
|4|6.4|11.4|15.1|19|19.4|

### Arithmetic circuits

|Threads|100 Mbps|500 Mbps|1 Gbps|2 Gbps|Localhost|
|-------|-------|-------|-------|-------|---------|
|1|1.4|4.8|6.8|7.8|7.8|
|2|1.4|5.6|8.7|10.2|10.4|
|3|1.4|5.9|9.3|11.7|12.5|

(The multi-thread columns reflect the v0.3.x engine and do not apply to the
current single-threaded `main`.)

## [Acknowledgement, Reference, and Questions](https://github.com/emp-toolkit/emp-readme/blob/main/README.md#citation)

Please send email to Xiao Wang (wangxiao1254@gmail.com).

## License

Licensed under the Apache License, Version 2.0 — see [LICENSE](LICENSE).
