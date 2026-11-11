#ifndef EMP_ZK_TYPES_H__
#define EMP_ZK_TYPES_H__

// The emp-zk-bool statement-boundary value types and the helpers that bridge
// them to the engine's raw `block` world. ZKBit / ZKInt are emp-tool circuit
// values built over ZKBoolContext; a user (and the gadgets, at their public
// surface) expresses statements with these, while the proof gadgets
// (polynomial / perm / f2k) keep operating on raw authenticated blocks.
//
// ZKInt / ZKUInt are runtime-width (DynamicInt_T / DynamicUInt_T<Ctx>): width is a
// runtime value and they are intentionally NOT fixed-width WireValues (no static
// width()/clear codec) — they model RuntimeWidthValue instead. Build/open through
// ZKBoolSession's runtime overloads sess.input<ZKUInt>(owner, value, width) /
// sess.reveal(x, recipient). ZKUInt reveals as uint64_t (zero-extend); ZKInt as
// int64_t (two's-complement sign-extend) — pick by the value's signedness.
//
// Boundary helpers COPY the block out of / into a value (a ZKWire is layout-
// identical to a block — asserted in zk_context.h — but a reinterpret_cast view
// would not be strict-aliasing-safe, and this is not the hot path).

#include "emp-zk/emp-zk-bool/zk_context.h"
#include "emp-tool/circuits/typed.h"   // Bit_T / Int_T / UInt_T / value_traits
#include <vector>

namespace emp {

using ZKBit  = Bit_T<ZKBoolContext>;
using ZKInt  = DynamicInt_T<ZKBoolContext>;   // runtime-width signed integer (two's-complement)
using ZKUInt = DynamicUInt_T<ZKBoolContext>;  // runtime-width unsigned integer (zero-extend)

// --- boundary helpers -------------------------------------------------------

// The authenticated block of a single bit.
inline block bit_block(const ZKBit& b) { return b.w.label; }

// Copy the per-bit authenticated blocks (LSB-first) out of a runtime-width int.
// Generic over the runtime-width value type (ZKInt or ZKUInt) — it reads only the
// wires, which are identical regardless of the value's signedness.
template <class V>
inline std::vector<block> int_blocks(const V& x) {
    const int n = x.width();
    std::vector<block> out((std::size_t)n);
    for (int i = 0; i < n; ++i) out[(std::size_t)i] = x.data()[i].label;
    return out;
}

inline int zk_int_width(const ZKInt& x) { return x.width(); }

// Rebuild a runtime-width int from raw authenticated blocks (LSB-first).
inline ZKInt zk_int_from_blocks(ZKBoolContext& ctx, const block* blk, int width) {
    std::vector<ZKWire> wires((std::size_t)width);
    for (int i = 0; i < width; ++i) wires[(std::size_t)i].label = blk[i];
    return ZKInt::from_wires(ctx, wires.data(), width);
}

}  // namespace emp
#endif  // EMP_ZK_TYPES_H__
