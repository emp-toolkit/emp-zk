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

### Security

These fix proofs that a cheating prover could make the verifier accept, or
that leaked witness data to the verifier, in `v1.0.0-alpha.1` and
`v1.0.0-alpha.2`. A proof made there with `ZKROM`, with `ZKRam` at a bound `T`
that is not a power of two or past `T` accesses, or with `ZKSet` past `T`
queries of one value, is not sound against a cheating prover.

- `ZKROM` accepted any lookup value. A lookup wrote its record back stamped
  with the public clock, and nothing constrained the time the lookup claimed
  to read, so a lookup could read back its own write, with any value at any
  index. A lookup now writes back the version it read plus one (Yang–Heath,
  "Two Shuffles Make a RAM", Fig. 7), so the versions of a cell form one chain
  from its setup write.
- `ZKRam` accepted reads of a later write whenever `T` was not a power of two.
  Its freshness check computed clock − t modulo 2^⌈log₂(T+1)⌉, which is less
  than 2T, so the difference for a later write could wrap into {1, …, T}. The
  time field now has ⌈log₂ 2T⌉ bits.
- `ZKRam` and `ZKROM` did not enforce `T`. Past `T` accesses, forged reads
  passed even at a power-of-two `T`. An access past `T` now aborts on both
  parties.
- `ZKSet` accepted a value outside its range after 2^ver_sz queries of it,
  whose versions then closed a cycle. The version field was sized from the
  set's size, and nothing bounded the number of queries. `ZKSet` now takes a
  `max_queries` bound (default `T`), sizes its versions from it, and aborts
  past it.
- `ZKPermProof` sent both evaluated products p_A(r) and p_B(r) under MACs the
  verifier can open, so a one-element multiset revealed its element. The
  prover now sends only the difference of the two MACs.
- The permutation proof's product check sent its folded coefficients
  unmasked, so a verifier could test guesses of a low-entropy witness. They
  are now masked by four random authenticated values (512 COTs per check).

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
- `BoolIO`'s receiver checks that the padding of each closed chunk is all
  true bits. The transcript absorbs the padding, so a sender could otherwise
  vary bits that the receiver ignores.
- `BoolIO` takes its sender flag as a `bool`. Passing a party id, which
  would make both parties senders, no longer compiles. The proof engine
  checks the flag against its party before any traffic.
- `ZKBoolSession::input` (runtime width) and `zk_int_from_blocks` take the
  width as `int64_t` and reject a width outside [1, INT_MAX].
- The `zkp_*` polynomial gadgets reject a negative length.
- The f2k multiplication check holds no cleartext buffers on the verifier and
  folds its coefficients in chunks. The `f2k_input` buffer is allocated on the
  first `f2k_input`.
- The polynomial proof's prover accumulates its terms with masks, not branches
  on witness bits.
- New tests check that a cheating prover is rejected by each Boolean check:
  - the AND-gate batch, at every batch boundary;
  - the revealed-value digest;
  - the f2k multiplication check, in its first and in its last chunk;
  - all four polynomial statements;
  - `BoolIO`'s padding check and its sender-flag check.

  They also run honest proofs across a SilentFerret round, both streaming and
  with a prepay of several rounds.
- The `bool_io` test streams 4M bits instead of 100M.
- The permutation proof's product check folds the products with independent
  coefficients. With powers of one challenge the fold alone could err with
  probability up to about 2⁻¹¹² for a full batch; it now errs with about
  2⁻¹²⁸, and a batch with about 2⁻¹²⁴, which is its degree-5 test in Δ. Its
  seed, the fold coefficients and the challenge r come from the transcript
  under per-use domains and the session id, like the Boolean checks.
- A permutation proof multiplies only through its product proof: the last
  product of each side fills its free slots with a public 1, where the last
  one to three factors used `f2k_mul`. A session that runs only RAM, ROM, set
  and permutation proofs no longer starts the f2k VOLE.
- `ZKBoolSession::finalize()` aborts while a RAM, ROM, set or permutation
  proof holds records that its check has not checked, and so does destroying
  one of them before its check. `ZKSet` and `ZKPermProof` are no longer
  movable.
- `ZKRam` keeps the prover's copy of the memory as bits (`mem` is a `bool`
  array), so values may be wider than 64 bits.
- `ZKRam`, `ZKSet` and `ZKPermProof` check their parameters: index and value
  widths, at most 2^index_sz cells, an element width that holds `T` (and at
  most 64 bits), and positive sizes and bounds. `ZKRam` needs `init()` before
  any access, and `ZKRam` and `ZKSet` take one `check()` and no access after
  it.
- The verifier's abort message names the gadget whose records do not match:
  `ZKRam`, `ZKSet`, or `ZKPermProof`, which takes the message as an optional
  constructor argument.
- `ZKRam::check()` commits each cell's final value before its time in a fixed
  order, rather than in the order a compiler evaluates function arguments, so
  a prover and a verifier built by different compilers agree.
- `RamPolyPrdt` takes the engine and proves products of exactly five values
  (`polyPrdt5`). A product of three or four values, which `polyPrdtN` allowed
  but no caller used, would put its error on a power of Δ that the prover's
  coefficients cancel.
- New tests check that the RAM, ROM, set and permutation proofs reject a
  cheating prover: reads of a later write at a `T` that is not a power of two,
  a stale read after a store, a changed value (also above bit 128), a ROM
  lookup that reads its own write and a cycle of forged lookups, an index past
  the last cell, more than `T` accesses in a RAM and in a ROM, a set query past
  its bound and a non-member whose versions would wrap, false permutations of
  one-wire and three-wire elements, a wrong product alone and in the first of
  two batches, a missing `check()`, each parameter contract, and the
  transcript of the open and the product check. The checks on public data are
  tested on the verifier's copy alone. `ram_test` and `rom_test` fail on a
  wrong read.

### Removed

- `ZKBoolBase::authenticated_input_bits_zero_extend`. Its only caller,
  `f2k_pack_v`, commits its bits directly.
- `RamPolyPrdt::polyPrdtN`, replaced by `polyPrdt5`.
- The `bad` modes of the RAM, ROM, set and permutation tests, which no test
  ran. `test/ram-zk/cheat.cpp` covers them.

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
