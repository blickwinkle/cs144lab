#include "tcp_sender.hh"
#include "buffer.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_sender_message.hh"
#include "wrapping_integers.hh"

#include <cstddef>
#include <cstdint>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : ack_record_ { 0, 0 }
  , msg_()
  , isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , RTO_ms_( initial_RTO_ms )
  , timer_()
  , consecutive_retransmissions_( 0 )
  , buffer_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return min( this->buffer_.size() - this->ack_record_.last_ack_received_ + 1,
              max( this->ack_record_.last_ack_window_size_, (uint64_t)1 ) );
  // uint64_t seqNo = this->msg_.empty()
  //                    ? this->ack_record_.last_ack_received_
  //                    : this->msg_.back().seqno.unwrap( this->isn_, this->ack_record_.last_ack_received_ )
  //                        + this->msg_.back().sequence_length();
  // uint64_t payload_idx = seqNo;
  // if ( payload_idx > 0 )
  //   payload_idx--;
  // return this->buffer_.size() - payload_idx + seqNo == 0;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return this->consecutive_retransmissions_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if ( this->timer_.isExpir() ) {
    if ( this->calc_remain_wsize() != 0 ) {
      this->consecutive_retransmissions_++;
      this->RTO_ms_ *= 2;
    }
    if ( this->consecutive_retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS ) {
      return {};
    }

    this->timer_.close();
    this->timer_.start( this->RTO_ms_ );
    return this->msg_.front();
  }
  uint64_t seqNo = this->msg_.empty()
                     ? this->ack_record_.last_ack_received_
                     : this->msg_.back().seqno.unwrap( this->isn_, this->ack_record_.last_ack_received_ )
                         + this->msg_.back().sequence_length();
  uint64_t payload_idx = seqNo;
  if ( payload_idx > 0 )
    payload_idx--;
  uint64_t payload_size = min(
    min( this->calc_remain_wsize(), this->buffer_.size() >= payload_idx ? this->buffer_.size() - payload_idx : 0 ),
    TCPConfig::MAX_PAYLOAD_SIZE );

  if ( payload_size == 0 && seqNo != 0 )
    return {};

  TCPSenderMessage message { .seqno = Wrap32::wrap( seqNo, this->isn_ ),
                             .SYN = seqNo == 0,
                             .payload
                             = { payload_size != 0 ? this->buffer_.substr( payload_idx, payload_size ) : "" },
                             // .FIN = payload_idx + payload_size >= buffer_.size(),
                             .FIN = false };
  this->msg_.push( message );
  if ( !this->timer_.isTiming() ) {
    this->timer_.start( this->RTO_ms_ );
  }
  return message;
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  // (void)outbound_stream;
  string buf_;
  read( outbound_stream, outbound_stream.bytes_buffered(), buf_ );
  this->buffer_ += buf_;
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  uint64_t seqNo = this->msg_.empty()
                     ? this->ack_record_.last_ack_received_
                     : this->msg_.back().seqno.unwrap( this->isn_, this->ack_record_.last_ack_received_ )
                         + this->msg_.back().sequence_length();
  return { Wrap32::wrap( seqNo, this->isn_ ), false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  uint64_t ackno = 0;
  if ( msg.ackno.has_value() ) {
    ackno = msg.ackno.value().unwrap( this->isn_, this->ack_record_.last_ack_received_ );
  }
  if ( ackno <= this->ack_record_.last_ack_received_ ) {
    return;
  }
  this->ack_record_.last_ack_received_ = ackno;
  this->ack_record_.last_ack_window_size_ = msg.window_size;
  this->consecutive_retransmissions_ = 0;
  this->RTO_ms_ = this->initial_RTO_ms_;

  this->GC_buffer();
  if ( !this->msg_.empty() ) {
    this->timer_.start( this->RTO_ms_ );
  }
  return;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  this->timer_.addTime( ms_since_last_tick );
  return;
}

void TCPSender::GC_buffer()
{
  if ( this->msg_.empty() )
    return;
  while ( !msg_.empty() ) {
    auto tmpSeq = msg_.front().seqno.unwrap( this->isn_, this->ack_record_.last_ack_received_ );
    if ( tmpSeq + msg_.front().sequence_length() > this->ack_record_.last_ack_received_ ) {
      break;
    }
    msg_.pop();
  }
  return;
}

uint64_t TCPSender::calc_remain_wsize() const
{
  if ( this->msg_.empty() ) {
    return this->ack_record_.last_ack_window_size_;
  }
  return this->ack_record_.last_ack_window_size_
         - ( ( this->msg_.back().seqno.unwrap( this->isn_, this->ack_record_.last_ack_received_ )
               + this->msg_.back().sequence_length() )
             - this->ack_record_.last_ack_received_ );
}