#include "router.hh"
#include "address.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  this->rules_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

void Router::route()
{
  // For each interface, use the maybe_receive() method to consume every incoming datagram and
  // send it on one of interfaces to the correct next hop. The router
  // chooses the outbound interface and next-hop as specified by the
  // route with the longest prefix_length that matches the datagram's
  // destination address.
  for ( size_t i = 0; i < this->interfaces_.size(); i++ ) {
    auto& interface = this->interface( i );

    auto maybe_datagram = interface.maybe_receive();
    if ( !maybe_datagram.has_value() ) {
      continue;
    }
    auto datagram = maybe_datagram.value();
    if ( datagram.header.ttl <= 1 ) {
      continue;
    }
    datagram.header.ttl--;
    datagram.header.compute_checksum();

    auto destination_address_numeric = datagram.header.dst;
    auto best_rule = this->rules_.end();
    for ( auto rule = this->rules_.begin(); rule != this->rules_.end(); rule++ ) {
      if ( rule->is_match( destination_address_numeric ) ) {
        if ( best_rule == this->rules_.end() ) {
          best_rule = rule;
        } else if ( rule->prefix_length() > best_rule->prefix_length() ) {
          best_rule = rule;
        }
      }
    }
    if ( best_rule == this->rules_.end() ) {
      cerr << "DEBUG: no route for " << Address::from_ipv4_numeric( destination_address_numeric ) << "\n";
      continue;
    }
    auto& best_rule_ref = *best_rule;
    if ( best_rule_ref.next_hop().has_value() ) {
      // cerr << "DEBUG: forwarding " << destination_address.ip() << " to " << best_rule_next_hop->ip() << "\n";
      // interface.send_datagram( *best_rule_next_hop, datagram.payload() );
      this->interface( best_rule_ref.interface_num() ).send_datagram( datagram, best_rule_ref.next_hop().value() );

    } else {
      // datagram.header.
      this->interface( best_rule_ref.interface_num() )
        .send_datagram( datagram, Address::from_ipv4_numeric( destination_address_numeric ) );
    }
  }
}
