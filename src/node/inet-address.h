#ifndef IPV4_TRANSPORT_ADDRESS_H
#define IPV4_TRANSPORT_ADDRESS_H

#include "address.h"
#include "ipv4-address.h"
#include <stdint.h>

namespace ns3 {

class InetAddress
{
public:
  InetAddress (Ipv4Address ipv4, uint16_t port);
  InetAddress (Ipv4Address ipv4);
  InetAddress (uint16_t post);
  uint16_t GetPort (void) const;
  Ipv4Address GetIpv4 (void) const;

  void SetPort (uint16_t post);
  void SetIpv4 (Ipv4Address address);

  Address ConvertTo (void) const;
  static InetAddress ConvertFrom (const Address &address);
private:
  static uint8_t GetType (void);
  Ipv4Address m_ipv4;
  uint16_t m_port;
};

} // namespace ns3


#endif /* IPV4_TRANSPORT_ADDRESS_H */
