#ifndef __EMP_BITIO_H__
#define __EMP_BITIO_H__
#include "emp-tool/emp-tool.h"

namespace emp {
using namespace std;
// The channel of an emp-zk proof. It wraps the connection's channel `io` and
// carries two kinds of traffic over it in protocol order: single bits
// (send_bit / recv_bit), buffered and sent packed 8 per byte in chunks of
// NETWORK_STAGING_BUFFER_SIZE bits, and byte messages (everything sent through
// this object as an IOChannel, including by the OT and VOLE extensions given
// this channel).
//
// Bits flow one way, from the prover (sender) to the verifier. A chunk crosses
// the wire when it is full, so a byte message in the middle of a chunk would
// overtake the bits before it. Every byte message therefore closes the current
// chunk first, on both sides: the sender pads the partial chunk and sends it,
// and the receiver drops the unread padding of the chunk it holds. Both
// parties reach each byte message at the same protocol point, so they close
// the same chunk.
//
// Everything goes through `io`, so this object uses io's Fiat-Shamir
// transcript (use_transcript_of) and keeps none of its own: the proof's checks
// and the extensions that turn the transcript on through this object share the
// one transcript, which sees the bits and the bytes in wire order. Read the
// digest after flush(): until then the sender's transcript lacks the bits of
// its partial chunk, which the receiver's will include once the chunk arrives.
// Session ids come from `io` too: the two are one connection, so they share
// one sequence.
class BoolIO : public IOChannel {
public:
  IOChannel *io;
  // Staging buffer of unpacked bools, flushed via the inner channel's
  // send_bool / recv_bool (which pack 8 bools per wire byte). Raw bool[]
  // (not vector<bool>, which is bit-packed) so we hand send_bool a real bool*.
  std::unique_ptr<bool[]> buf;
  int ptr;
  bool sender;
  BoolIO(IOChannel *io, int sender) : io(io), sender(sender) {
    use_transcript_of(io);
    // Exactly one party passes true (sender = party == ALICE). Guard so
    // re-wrapping a channel that already has its transcript on keeps it.
    if (!io->fs_enabled())
      io->enable_fs(sender);
    buf.reset(new bool[NETWORK_STAGING_BUFFER_SIZE]);
    if (sender)
      ptr = 0;
    else
      ptr = NETWORK_STAGING_BUFFER_SIZE;
  }
  ~BoolIO() { this->flush(); }
  void flush() override {
    close_chunk_();
    io->flush();
  }

  bool has_session_root() const override { return io->has_session_root(); }
  [[nodiscard]] SessionID next_session_id() override {
    return io->next_session_id();
  }

  void send_bit(bool data) {
    buf[ptr] = data;
    ptr++;
    if (ptr == NETWORK_STAGING_BUFFER_SIZE) {
      io->send_bool(buf.get(), NETWORK_STAGING_BUFFER_SIZE);
      ptr = 0;
    }
  }

  bool recv_bit() {
    if (ptr == NETWORK_STAGING_BUFFER_SIZE) {
      io->recv_bool(buf.get(), NETWORK_STAGING_BUFFER_SIZE);
      ptr = 0;
    }
    bool res = buf[ptr];
    ptr++;
    return res;
  }

  // This relies on `io` sending what is pending before it blocks on a
  // receive, as NetIO and TLSIO do: closing the chunk is then all a byte
  // message needs here.
  void send_data_internal(const void *data, int64_t nbyte) override {
    close_chunk_();
    io->send_data(data, nbyte);
  }

  void recv_data_internal(void *data, int64_t nbyte) override {
    close_chunk_();
    io->recv_data(data, nbyte);
  }

private:
  // Close the current chunk, as the class comment describes. Sends nothing
  // when the sender has no bits pending.
  void close_chunk_() {
    if (sender) {
      while (ptr != 0)
        send_bit(true);
    } else {
      ptr = NETWORK_STAGING_BUFFER_SIZE;
    }
  }
};
} // namespace emp
#endif // __EMP_BITIO_H__
