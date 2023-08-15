#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"

#include <cstdint>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return this->buffer_.size();
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return this->retransmission_timeout_counter_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if (this->needRetransmission_) {
    this->needRetransmission_ = false;
    this->consecutive_retransmissions_++;
    return this->message_;
  }
  if (this->buffer_.empty()) {
    return {};
  }
  this->message_.seqno = Wrap32::wrap(this->last_seqno_sent_ + this->message_.sequence_length(), this->isn_);
  

}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  // (void)outbound_stream;
  read(outbound_stream, outbound_stream.bytes_buffered(), buffer_);
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return {Wrap32::wrap(this->last_ack_received_, this->isn_), false, {}, false};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  uint64_t ackno = 0;
  if (msg.ackno.has_value()) {
    ackno = msg.ackno.value().unwrap(this->isn_, this->last_seqno_sent_);
  }
  if (ackno <= this->last_ack_received_) {
    return ;
  }
  this->last_ack_received_ = ackno;
  this->last_window_size_ = msg.window_size;
  this->consecutive_retransmissions_ = 0;
  this->RTO_ms_ = this->initial_RTO_ms_;
  this->retransmission_timeout_counter_ = 0;
  this->needRetransmission_ = false;
  return ;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  this->retransmission_timeout_counter_ += ms_since_last_tick;
  if (this->retransmission_timeout_counter_ >= this->RTO_ms_) {
    this->retransmission_timeout_counter_ = 0;
    this->consecutive_retransmissions_++;
    needRetransmission_ = true;
    this->RTO_ms_ *= 2;
  }
  return ;
}
