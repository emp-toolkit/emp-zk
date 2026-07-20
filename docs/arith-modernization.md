# Arithmetic modernization (roadmap)

Status: **planned for the `v1.0.0-alpha.2` cycle.** Not started. This
document is the design of record; the code still ships the pre-migration
global-singleton arithmetic backend described in [AGENTS.md](../AGENTS.md).

## Goal and scope

The Boolean side (`emp-zk-bool`) was modernized from a global-singleton
backend to an explicit `ZKBoolSession` handle that conforms to emp-tool's
`BooleanContext` / `Session` concepts. The arithmetic side
(`emp-zk-arith`) is still in the pre-migration state: a global
`ZKFpExec::zk_exec` singleton, two more globals
(`FpPolyProof::fppolyproof`, `EdaBits::conv`), and
`setup_zk_arith` / `finalize_zk_arith` free functions.

This is a **half modernization**: de-globalize the arithmetic backend and
give it an explicit session, plus add a *minimal, reusable* arithmetic-ops
concept to emp-tool — but **no arithmetic circuit**.

**In scope**

- A minimal, batched, field/ring-agnostic **`ArithmeticContext` concept in
  emp-tool** (the algebra a session must support), reusable by other
  arithmetic MPC protocols, not emp-zk-private.
- An emp-zk **`ZKFpSession`** (I/O + lifecycle) owning a thin **`ZKFpContext`**
  (algebra) that conforms to the concept; removal of all three globals and
  the setup/finalize free functions.

**Out of scope (explicitly rejected)**

- A typed field WireValue (`Fp_T`) with circuit recording, arithmetic IR, a
  frontend, or `.empbc` assets. Arithmetic here is a special-purpose library
  (inner-product and polynomial gadgets, private multiplication, the edabit
  bridge; matrix multiply and SIS are tested example constructions); it has no
  compile-once / run-anywhere need, unlike Boolean (the universal circuit
  substrate).
- Any change to the wire format, algebra, or soundness. This is a
  plumbing/lifetime refactor plus the emp-tool concept. Note the
  *deterministic transcript will still move*: removing the unconditional
  pre-draw and centralizing/reordering correlation draws changes the byte
  trace under `EMP_TEST_MODE` even though the protocol stays equivalent — so
  the phase-0 baseline (below) is a reviewable regression aid, regenerated
  with the delta reviewed on an intentional change, not a freeze.

## Layering

Mirrors emp-tool's `BooleanContext` → `DirectSession` → `SessionIO`, with a
clean split: **the context handles algebra, the session handles I/O
(including conversion).**

```
ArithmeticContext   (emp-tool concept)  — pure algebra: public/add/mul/mul_const,
                                          batched, field/ring-agnostic; no I/O, no party()
      ▲ satisfied by
ZKFpContext         (emp-zk)            — thin algebra view over the engine;
                                          what GADGETS receive (least authority)
      ▲ owned + exposed via ctx() by
ZKFpSession         (emp-zk)            — party() + input/reveal + conversion(edabit) + finalize
```

- A gadget (`fp_zkp_inner_prdt`, matrix multiply, …) receives the
  `ZKFpContext&` — algebra only, so it structurally cannot perform I/O or
  conversion. Only the top-level proof driver holds the `ZKFpSession`.
- **Conversion is I/O.** The edabit bool↔arith bridge communicates and
  touches both domains, so `bool2arith` / `arith2bool` are `ZKFpSession`
  methods (via a borrowed `ZKBoolSession`) — never in the concept, never a
  global.

## The emp-tool `ArithmeticContext` concept

Add `emp-tool/ir/session/arithmetic_session.h`, exported from `ir/ir.h`.
It is the arithmetic analog of `BooleanContext`: pure batched algebra, no
application I/O, no `party()`. ("No I/O" means no input / reveal / domain
conversion — the algebra ops may still communicate as part of the protocol,
exactly as `BooleanContext::and_gate` does.) The existing
`BulkBooleanContext::and_many` is the precedent for documenting a
batching-complexity contract the C++ concept itself cannot enforce.

Required operations (all batched over spans; names indicative):

- `public_values(out, clear)` — materialize public constants.
- `add_many(out, a, b)` — local, communication-free.
- `neg_many(out, a)` — ring additive inverse; local.
- `mul_many(out, a, b)` — consumes correlations; the multiplication layer.
- `mul_const_many(out, a, clear)` — local.

Associated types: `arith_value_t` (share/label) and `arith_clear_t`
(cleartext value), both `semiregular`.

**Field/ring-agnostic — do NOT assume a prime field.** Element types are
associated types (never a hardcoded `uint64_t` / `mod p`), and
inversion/division are excluded, so the concept is a *commutative-ring*
interface (add / mul / additive-inverse / `mul_const` / constant) that fits
a prime field (2⁶¹ − 1), GF(2ᵏ), or ℤ_{2ᵏ} alike. Subtraction is `add_many`
composed with `neg_many`; `neg_many` is the ring additive inverse, not
"mul by p − 1" (which would assume the prime-field backend) and not
`mul_const(−1)` (−1 is not a generic literal for an opaque `arith_clear_t`).

**Normative batching contract** (documentation, since the concept only
checks signatures): the local ops (`add_many`, `neg_many`, `mul_const_many`,
`public_values`) perform no communication and consume no correlations; one
`mul_many` over N
elements uses **O(1) online communication rounds** (bytes/correlations may
be O(N)); repeated length-one calls are not guaranteed to coalesce, so
generic code batches an independent layer into one span call.

Keep `input`, `reveal`, `finalize`, and reservation **out** of the concept —
those are I/O / lifecycle policy, not universal algebra, and live on the
concrete session.

emp-zk's `ZKFpSession` is the 2⁶¹ − 1 instantiation
(`arith_clear_t = uint64_t`); the interface does not preclude a GF(2ᵏ) or
ℤ_{2ᵏ} session conforming later, which is the whole point of putting the
concept in emp-tool.

## Value type

`IntFp` stays a bare 16-byte `AuthValueFp` (value in the low 64 bits, MAC in
the high 64), with **no per-value context/session pointer**. This is the key
divergence from Boolean, whose `ZKBit` carries a `ctx_` pointer and therefore
must be copied to `block` at every gadget boundary. Because `IntFp` is bare,
`std::span<IntFp>` is directly the operand layout the batched ops and gadgets
consume — **zero-copy**; the `ZKFpContext` is passed as a separate argument,
not embedded in each value. (Arithmetic is dominated by dense `IntFp[]`
arrays — matrix multiply, SIS, polynomial — so a per-value pointer would
bloat every element and break these arrays.)

Drop the current unsafe `IntFp*` → `__uint128_t*` array casts
(`int_fp.h`); use real `AuthValueFp` buffers. Add
size/alignment/standard-layout/offset `static_assert`s beside `IntFp`.
`assert_equal_many` / `assert_zero_many` should return `void` — the current
`batch_reveal_check` always returns `true`, which is a smell.

## Ownership and settlement

```
caller-owned BoolIO
└── ZKFpSession
    ├── role-specific ZKFpExec  →  FpOSTriple  →  correlation source (FpVOLE) + FpAuthHelper
    ├── FpPolyProof             (borrows the correlation source)
    └── optional EdaBits        (borrows the correlation source + a caller-owned ZKBoolSession)
```

Owning edges are `unique_ptr`; borrowing edges are references. The three
globals (`ZKFpExec::zk_exec`, `FpPolyProof::fppolyproof`, `EdaBits::conv`)
are removed. `EdaBits` is constructed **only** by the bridge constructor
(plain arithmetic proofs must not pay its upfront VOLE / Boolean cost).

`finalize()` is idempotent (destructor calls it as a fallback) and executes
a **teardown order that is protocol-significant**: `FpPolyProof` draws a
correlation during its own destruction and `EdaBits` runs its closing
equality digest, both before the engine ends the VOLE:

1. `EdaBits` (its equality digest runs at teardown),
2. `FpPolyProof` (flushes its pending batch, consumes a correlation),
3. the executor / `FpOSTriple` (checks pending multiplications, settles
   revealed-output MACs, then ends VOLE).

Every revealed value is **provisional until `finalize()` succeeds**; a
malicious prover is caught there, not necessarily at each gate. For a mixed
bool+arith proof, the current tests finalize the Boolean session first
(`sess.finalize()`) then the arithmetic side (`finalize_zk_arith()`); the
Boolean object stays in scope but its engine is already spent. Whether that
order is right given the edabit bridge borrows the Boolean session should be
settled during migration (see `test/arith/abconversion.cpp`); changing it is
a separate transcript-composition change.

## Round reduction and correlation reservation

The motivation for the batched interface is **MPC round reduction**: a batch
of N multiplies should cost O(1) online rounds, not N.

Precise hazard: consecutive corrections are already one same-direction run
under emp-tool's round counter, so the cost is a **direction reversal** —
VOLE-extension traffic is BOB→ALICE while multiplication corrections are
ALICE→BOB, so a correlation refill or a correctness check *inside* a batch
reverses direction and costs a round. Therefore:

- `mul_many(N)` must **reserve/draw all N correlations up front**, do one
  contiguous correction exchange, append all N triples to the pending-check
  buffer, and **never check or refill inside the batch**. If N exceeds the
  current `CHECK_SZ` capacity, grow the pending storage for that batch rather
  than splitting it — trade memory for the O(1) online-phase count.
- Remove `FpOSTriple`'s unconditional 1,048,576-correlation pre-draw; replace
  it with reservation.
- **Centralize all correlation draws** behind one source object. Today
  `FpPolyProof` and `EdaBits` bypass accounting and draw from the raw VOLE;
  reservation is unsound unless every draw (inputs, mul-outputs, check-masks,
  polynomial, edabit) funnels through one source.

**Reservation is not free here.** Unlike the Boolean side — where
`SilentFerret` provides a memory-light counted prepay (`begin(n)`, no per-COT
output storage) — arith's `FpVOLE` aliases a plain `Svole` / `Ferret`, which
has no counted prepay. So
`ZKFpSession`'s `expected_correlations` / `reserve_correlations(n)` must
**materialize ~16 bytes per reserved correlation**. A memory-light
`SilentSvole` in emp-ot would remove that cost but is a separate project, not
part of this migration. Under-reservation stays correct: the source
replenishes *before the next batch*, records a miss, and never replenishes
mid-batch.

## Migration precedent

The Boolean handle migration is commit **`f896deb`** (`emp-zk-bool`), not the
whole `v0.3.x → main` diff — earlier Boolean commits bundled an unrelated
engine-flatten / single-thread refactor; do **not** cargo-cult it (e.g. don't
restructure `FpOSTriple` merely because old Boolean `OSTriple` changed shape).
Mirror what `f896deb` actually did: retire the global registration, introduce
an owning noncopyable session with an idempotent finalizer + destructor
fallback, and make gadgets session/context-first. Arithmetic is *simpler* in
that it skips the per-value wire marshalling (bare `IntFp`).

## Phased plan (keep tests green at each step)

0. **Wire-transcript baseline gate.** Before touching code, capture
   deterministic directional transcript hashes + `IOChannel::rounds` / bytes /
   correlation counts for batched input, several sub-`CHECK_SZ` multiplies,
   polynomial proof, reveals, and mixed conversion. This is the regression
   guard for the whole refactor (the emp-zk analog of emp-ot's `trace_hash`) —
   a review aid whose baseline is regenerated, with the delta reviewed, when a
   phase intentionally changes correlation draw order; not a hard freeze.
1. **emp-tool concept.** Add `arithmetic_session.h`, its normative docs,
   umbrella export, a synthetic conforming mock, and negative concept probes
   (scalar-only / missing-op fails). Ships as emp-tool `v1.0.0-alpha.2`;
   emp-zk pins that exact tag.
2. **Batched engine internals, public API unchanged.** Add bulk
   `FpOSTriple` input / multiply / check with dynamic pending-check capacity
   and the centralized correlation source; test via `test/arith/ostriple.cpp`
   (already stack-owns `FpOSTriple`). Existing higher-level tests stay green.
3. **Atomic hard cut.** Add `ZKFpSession` + `ZKFpContext`; move the executor,
   polynomial proof, and edabit under the session; remove all three globals
   and `setup_zk_arith` / `finalize_zk_arith`; migrate every test and bench in
   the same commit. No compatibility shim (pre-1.0 alpha).
4. **Reservation + round guarantees.** Route every draw through the source,
   wire `expected_correlations` / `reserve_correlations`, add round-count
   tests comparing reserved N = 1 vs large-N `mul_many` (assert a bounded
   constant delta, with a small test-configured `CHECK_SZ` so the boundary is
   exercisable without a million-element CI test).
5. **Batch the workloads.** Tile independent products (matrix multiply), batch
   SIS squares and witness inputs; keep genuine dependency chains
   (`zk_proof.cpp`, the circuit-scalability bench) as size-1 "latency" tests.
6. **Verify.** Fast ctest after every phase, the slow edabit test after the
   ownership cut, transcript comparison against the phase-0 baseline,
   invalid-party/span/alias tests, double-finalize / destructor-fallback, and
   two independent sessions on separate channels.

## Principal risks

- **Correlation order** — one shared VOLE stream serves several gadgets;
  reordered or bypassed draws desynchronize the parties.
- **Teardown order** — polynomial and conversion finalizers consume the
  executor-owned correlation stream; destroy them before the engine.
- **Memory vs rounds** — an arbitrarily large logical batch needs
  correspondingly large pending-check / reserved-correlation storage.
- **Compact-value limitation** — without a context tag, mixing labels from
  different sessions cannot be detected; document it as invalid.
- **Public-label drift** — derive the public MAC once per session, not per
  batch, or authenticated public values silently change.
- **Aliasing** — exact in-place output must save inputs before overwriting;
  reject partial overlap.

## Provenance

This design was derived by comparing the Boolean `f896deb` migration against
the current arithmetic backend and cross-checking each concrete claim against
the code. Key settled decisions: context/session split (algebra vs
I/O-incl-conversion), field/ring-agnostic concept, compact `IntFp`,
materialized (non-free) reservation, and mirroring `f896deb` rather than the
whole branch diff.
