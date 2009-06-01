/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */

#include "ns3/assert.h" 
#include "ns3/node.h" 
#include "ns3/boolean.h"
#include "ipv4.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (Ipv4);

TypeId 
Ipv4::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Ipv4")
    .SetParent<Object> ()
    .AddAttribute ("IpForward", "If enabled, node can act as unicast router.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&Ipv4::SetIpForward,
                                        &Ipv4::GetIpForward),
                   MakeBooleanChecker ())
#if 0
    .AddAttribute ("MtuDiscover", "If enabled, every outgoing ip packet will have the DF flag set.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&UdpSocket::SetMtuDiscover,
                                        &UdpSocket::GetMtuDiscover),
                   MakeBooleanChecker ())
#endif
    ;
  return tid;
}

Ipv4::Ipv4 ()
{}

Ipv4::~Ipv4 ()
{}

} // namespace ns3
