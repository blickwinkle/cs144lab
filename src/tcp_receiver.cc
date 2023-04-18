#include "tcp_receiver.hh"
#include "wrapping_integers.hh"
#include <cstdint>
#include <optional>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN) {
    zero_point = message.seqno;
    has_syn = true;
  }
  if (!has_syn) {
    return;
  }
  reassembler.insert(message.seqno.unwrap(zero_point, inbound_stream.bytes_pushed()) - (!message.SYN && has_syn), message.payload, message.FIN, inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  // Your code here.
  if (!has_syn) {
    return {{}, static_cast<uint16_t>(inbound_stream.available_capacity() > UINT16_MAX ? UINT16_MAX : inbound_stream.available_capacity())};
  }
  return {Wrap32::wrap(inbound_stream.bytes_pushed() + has_syn + inbound_stream.is_closed(), zero_point), static_cast<uint16_t>(inbound_stream.available_capacity() > UINT16_MAX ? UINT16_MAX : inbound_stream.available_capacity())};
}
