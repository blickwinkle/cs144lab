#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "parser.hh"
#include <optional>
#include <utility>

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
  , arq_req_timeout_( 5 * 1000 )
  , arq_cache_timeout_( 30 * 1000 )
  , time( 0 )
  , arp_cache_()
  , buffer_()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame frame {
    .header { .dst = ETHERNET_BROADCAST, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_IPv4 },
    .payload = serialize( dgram ) };
  if ( this->arp_cache_.count( next_hop.ipv4_numeric() ) && this->arp_cache_[next_hop.ipv4_numeric()].work ) {
    frame.header.dst = this->arp_cache_[next_hop.ipv4_numeric()].addr;
  }
  this->buffer_.push_back( make_pair( std::move( frame ), next_hop ) );
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != this->ethernet_address_ ) {
    return {};
  }
  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage m;
    if ( !parse( m, frame.payload ) ) {
      return {};
    }
    if ( !m.supported() ) {
      return {};
    }
    this->arp_cache_[m.sender_ip_address]
      = NetworkInterface::ArpResponse { .work = true, .time = this->time, .addr = m.sender_ethernet_address };
    if ( this->ip_address_.ipv4_numeric() != m.target_ip_address || frame.header.dst != ETHERNET_BROADCAST ) {
      return {};
    }
    ARPMessage res { .sender_ethernet_address = this->ethernet_address_,
                     .sender_ip_address = this->ip_address_.ipv4_numeric(),
                     .target_ethernet_address = frame.header.src,
                     .target_ip_address = m.sender_ip_address };
    EthernetFrame send_frame {
      .header { .dst = frame.header.src, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP },
      .payload = serialize( res ) };
    this->buffer_.push_back( make_pair( std::move( send_frame ), std::nullopt ) );
    return {};
  } else if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram idata;
    if ( !parse( idata, frame.payload ) ) {
      return {};
    }
    return idata;
  } else {
    return {};
  }
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  this->time += ms_since_last_tick;
  gc();
}

void NetworkInterface::gc()
{
  for ( auto& x : this->arp_cache_ ) {
    if ( x.second.work && this->time - x.second.time >= this->arq_cache_timeout_ ) {
      x.second = {};
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  for ( auto iter = this->buffer_.begin(); iter != this->buffer_.end(); iter++ ) {
    if ( iter->first.header.type == EthernetHeader::TYPE_ARP ) {
      EthernetFrame m = std::move( iter->first );
      this->buffer_.erase( iter );
      return m;
    }
    if ( this->arp_cache_[iter->second->ipv4_numeric()].work ) {
      EthernetFrame m = std::move( iter->first );
      m.header.dst = this->arp_cache_[iter->second->ipv4_numeric()].addr;
      this->buffer_.erase( iter );
      return m;
    } else if ( this->arp_cache_[iter->second->ipv4_numeric()].sended
                && this->arp_cache_[iter->second->ipv4_numeric()].time - this->time < this->arq_req_timeout_ ) {
      continue;
    } else {
      ARPMessage req { .sender_ethernet_address = this->ethernet_address_,
                       .sender_ip_address = this->ip_address_.ipv4_numeric(),
                       .target_ethernet_address = ETHERNET_BROADCAST,
                       .target_ip_address = iter->second->ipv4_numeric() };
      this->arp_cache_[iter->second->ipv4_numeric()]
        = { .work = false, .time = this->time, .addr = ETHERNET_BROADCAST, .sended = true };

      return EthernetFrame {
        .header { .dst = ETHERNET_BROADCAST, .src = this->ethernet_address_, .type = EthernetHeader::TYPE_ARP },
        .payload = serialize( req ) };
    }
  }
  return {};
}
