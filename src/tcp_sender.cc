#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "tcp_config.hh"

#include <cstdint>
#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) )
  , initial_RTO_ms_( initial_RTO_ms )
  , RTO_ms_( initial_RTO_ms )
  , retransmission_timeout_counter_( 0 )
  , consecutive_retransmissions_( 0 )
  , last_ack_received_( 0 )
  , last_seqno_sent_( 0 )
  , last_window_size_( 0 )
  , local_buffer_idx_( 0 )
  , needRetransmission_( false )
  , has_syn_( false )
  , has_fin_( false )
  , has_send_fin_( false )
  , buffer_()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return this->buffer_.size() + !this->ack_record_.has_ack_syn_ + !this->ack_record_.has_ack_fin_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return this->retransmission_timeout_counter_;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  // Your code here.
  if ( this->needRetransmission_ ) {
    this->needRetransmission_ = false;
    this->consecutive_retransmissions_++;
    if ( this->consecutive_retransmissions_ > TCPConfig::MAX_RETX_ATTEMPTS ) {
      return {};
    }
    uint64_t payload_size
      = min( min( this->ack_record_.last_ack_window_size_, this->buffer_.size() ), TCPConfig::MAX_PAYLOAD_SIZE );
    bool is_fin
      = this->ack_record_.last_ack_received_ + payload_size >= this->buffer_.size() + this->local_buffer_idx_;
    return this->construct_message(
      this->ack_record_.last_ack_received_, payload_size, !this->ack_record_.has_ack_syn_, is_fin );
  }

  if ( this->last_seqno_sent_ >= this->buffer_.size() + this->local_buffer_idx_ ) {
    return {};
  }
  uint64_t payload_size = min( min( this->last_window_size_ - ( this->last_seqno_sent_ - this->last_ack_received_ ),
                                    this->buffer_.size() - ( this->last_seqno_sent_ - this->local_buffer_idx_ ) ),
                               TCPConfig::MAX_PAYLOAD_SIZE );
  bool is_fin
    = this->has_fin_ && this->last_seqno_sent_ + payload_size >= this->buffer_.size() + this->local_buffer_idx_;

  TCPSenderMessage message
    = this->construct_message( this->last_seqno_sent_, payload_size, !this->has_syn_, is_fin );
  if ( !this->has_syn_ ) {
    this->has_syn_ = true;
  }
  if ( is_fin ) {
    this->has_send_fin_ = true;
  }
  this->last_seqno_sent_ += payload_size;
  return message;
}

void TCPSender::push( Reader& outbound_stream )
{
  // Your code here.
  // (void)outbound_stream;
  read( outbound_stream, outbound_stream.bytes_buffered(), buffer_ );
  this->has_fin_ = outbound_stream.is_finished();
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return { Wrap32::wrap( this->last_ack_received_ + this->has_syn_, this->isn_ ), false, {}, false };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  uint64_t ackno = 0;
  if ( msg.ackno.has_value() ) {
    ackno = msg.ackno.value().unwrap( this->isn_, this->last_seqno_sent_ );
  }
  if ( ackno <= this->last_ack_received_ ) {
    return;
  }
  this->last_ack_received_ = ackno;
  this->last_window_size_ = msg.window_size;
  this->consecutive_retransmissions_ = 0;
  this->RTO_ms_ = this->initial_RTO_ms_;
  this->retransmission_timeout_counter_ = 0;
  this->needRetransmission_ = false;
  this->GC_buffer();
  return;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  this->retransmission_timeout_counter_ += ms_since_last_tick;
  if ( this->retransmission_timeout_counter_ >= this->RTO_ms_ ) {
    this->retransmission_timeout_counter_ = 0;
    this->consecutive_retransmissions_++;
    needRetransmission_ = true;
    this->RTO_ms_ *= 2;
  }
  return;
}

TCPSenderMessage TCPSender::construct_message( uint64_t seqno, uint64_t size, bool is_syn, bool is_fin ) const
{
  TCPSenderMessage message;
  message.seqno = Wrap32::wrap( seqno + this->has_syn_, this->isn_ );
  message.SYN = is_syn;
  message.FIN = is_fin;
  message.payload = Buffer( this->buffer_.substr( seqno - this->local_buffer_idx_, size ) );
  return message;
}
void TCPSender::GC_buffer()
{
  if ( this->buffer_.empty() || this->local_buffer_idx_ >= this->last_ack_received_ ) {
    return;
  }
  this->buffer_ = this->buffer_.substr( this->last_ack_received_ - this->local_buffer_idx_ );
  this->local_buffer_idx_ = this->last_ack_received_;
  return;
}