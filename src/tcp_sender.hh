#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

class TCPTimer
{
private:
  bool isStart_;
  uint64_t outTime_;
  uint64_t expirTime_;

public:
  inline bool isTiming() const { return isStart_; }
  inline void start( uint64_t outTime ) { outTime_ = outTime, expirTime_ = 0, isStart_ = true; }
  inline bool isExpir() const { return isTiming() && expirTime_ >= outTime_; }
  inline void addTime( uint64_t time )
  {
    if ( !isTiming() ) {
      return;
    }
    expirTime_ += time;
  }
  inline void close() { isStart_ = false, outTime_ = expirTime_ = 0; }
};

class TCPSender
{
  struct
  {
    uint64_t last_ack_received_;
    uint64_t last_ack_window_size_;
  } ack_record_;
  std::queue<TCPSenderMessage> msg_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t RTO_ms_;
  TCPTimer timer_;
  uint64_t consecutive_retransmissions_;

  bool needRetransmission_;
  std::string buffer_;
  TCPSenderMessage construct_message( uint64_t seqno, uint64_t size, bool is_syn, bool is_fin ) const;
  uint64_t calc_remain_wsize() const;
  void GC_buffer();

public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};
