# Changelog

Notable changes to emp-zk. Versions are the git tags; CMake package
metadata is numeric (`project(VERSION)` cannot carry a prerelease suffix,
so the alpha status lives in the git tag).

## Unreleased

Requires emp-tool and emp-ot newer than `v1.0.0-alpha.2` (their `main`):
`BoolIO` calls emp-tool's `IOChannel::use_transcript_of`, and the OT and VOLE
extensions take their session ids from the channel. The session ids and the
changed Boolean checks below change every transcript, so both parties must
upgrade together.

### Changed

- The OT and VOLE extensions in a proof take fresh session ids from the
  connection, which both parties agree on, instead of the zero id: `BoolIO`
  forwards `has_session_root()` and `next_session_id()` to the channel it
  wraps.
- `BoolIO` keeps no Fiat-Shamir transcript of its own: it uses its channel's,
  which already sees all of the proof's traffic. The OT and VOLE extensions
  used to turn on a second transcript on the `BoolIO` itself, so the proof's
  byte traffic was hashed twice.
- `BoolIO` no longer flushes its channel on every byte message. The channel
  sends what is pending before it blocks on a receive, so the verifier's
  messages go out batched.
- The Boolean polynomial proofs (`zkp_poly_deg2`, `zkp_inner_prdt`,
  `zkp_inner_prdt_eq`, `zkp_inner_prdt_multi`) expand the verifier's seed into
  independent coefficients. Each batch check now errs with probability about
  2⁻¹²⁶. With powers of one challenge, a full batch could err with
  probability up to about 2⁻¹⁰⁸.
- The AND-gate and f2k batch checks derive their Fiat–Shamir seeds from the
  transcript digest under a per-check domain and a session id drawn at setup,
  instead of using the raw digest.
- `f2k_pack_v` commits the 128 bits it packs, one COT each. It used to commit
  130 and leave two of them unused.

### Removed

- `ZKBoolBase::authenticated_input_bits_zero_extend`. Its only caller,
  `f2k_pack_v`, commits its bits directly.

## v1.0.0-alpha.2

Second alpha of the 1.0 development line, pairing with emp-tool and emp-ot
`v1.0.0-alpha.2`, which it requires: `v1.0.0-alpha.1` does not build against
emp-tool `v1.0.0-alpha.2`. emp-ot's new Ferret and sVOLE parameters change
emp-zk's transcripts, so both parties must upgrade together.

### Changed

- `ZKInt` / `ZKUInt` are now emp-tool's `DynamicInt_T` / `DynamicUInt_T`,
  which emp-tool split out of `Int_T` / `UInt_T` for runtime-width
  integers. Code that uses the `ZKInt` / `ZKUInt` names is unchanged.
  Revealing one into a 64-bit integer now stops the program when the
  value does not fit, where it used to keep only the low 64 bits.
- `./run` uses `EMP_PORT` when set and 12345 otherwise, and `ctest` gives each
  two-party test its own port from 23000 up.

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
