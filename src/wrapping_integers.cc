#include "wrapping_integers.hh"
#include <cstdint>
#include <memory>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Convert absolute seqno → seqno. 
  // Given an absolute sequence number (n) and an Initial Sequence Number (zero point), produce the (relative) sequence number for n.
  return Wrap32(static_cast<uint32_t>( n + zero_point.raw_value_ ));

}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t ret = this->raw_value_ - zero_point.raw_value_;
  ret |= (checkpoint & 0xffffffff00000000);
  if ( ret > checkpoint && (checkpoint & 0xffffffff00000000)) {
    return checkpoint - (ret - 0x100000000) < ret - checkpoint ? ret - 0x100000000 : ret;
  }
  if ( ret < checkpoint) {
    return checkpoint - ret < ret + 0x100000000 - checkpoint ? ret : ret + 0x100000000;
  }
  return ret;
}
