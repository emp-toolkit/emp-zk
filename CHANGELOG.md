# Changelog

Notable changes to emp-zk. Versions are the git tags; CMake package
metadata is numeric (`project(VERSION)` cannot carry a prerelease suffix,
so the alpha status lives in the git tag).

## v1.0.0-alpha.1

First tagged alpha of the 1.0 development line, pairing with emp-tool and
emp-ot `v1.0.0-alpha.1`. The 0.3.x line is maintained separately and
receives backported fixes.

### Stability

- Pre-1.0 API: headers and names may change between alpha tags and before
  the final `1.0.0`. Pin to a specific tag, and pin the paired emp-tool /
  emp-ot tags.

### Contents

- Boolean ZK: native `BooleanContext` / `ZKBoolSession` (no global
  backend), driven by a `SilentFerret` COT engine with sized prepay.
  Typed values `Bit_T` / `UInt_T` / `Int_T` and runtime-width `ZKUInt` /
  `ZKInt`; circuits validate bit-for-bit against a cleartext-context replay.
- RAM / ROM / set-membership ZK gadgets (`ZKRam` / `ZKROM` / `ZKSet`,
  permutation proof) built on the Boolean session.
- Arithmetic ZK: a special-purpose algebraic library — `IntFp` over the
  field p = 2⁶¹ − 1 with private inputs, multiplication, and inner-product /
  polynomial gadgets, plus an edabit bool↔arith conversion bridge. (Matrix
  multiplication and SIS ship as tested example constructions.)
- Settlement contract: proof output is provisional until the session's
  closing checks — `ZKBoolSession::finalize()` on the Boolean side,
  `finalize_zk_arith()` on the arithmetic side.

### Requirements

- emp-ot `v1.0.0-alpha.1` (`find_package` version floor `1.0`) and its
  paired emp-tool; OpenSSL ≥ 3.0; CMake ≥ 3.21 (emp-tool needs ≥ 3.25).

### Security

Research software; no independent audit. The arithmetic ZK works over the
field p = 2⁶¹ − 1, so its single-field soundness is bounded near 2⁻⁶¹ —
not the 128-bit level of the Boolean side. Proof outputs are provisional
until the session's closing checks settle; a malicious prover is caught
there, not necessarily at each gate.
