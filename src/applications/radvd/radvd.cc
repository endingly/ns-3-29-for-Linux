/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 Telecom Bretagne
 * Copyright (c) 2009 Strasbourg University
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
 * Author: Sebastien Vincent <vincent@clarinet.u-strasbg.fr>
 *         Mehdi Benamor <benamor.mehdi@ensi.rnu.tn>
 */

#include "ns3/log.h"
#include "ns3/ipv6-address.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/net-device.h"
#include "ns3/uinteger.h"
#include "ns3/random-variable.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv6.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include "ns3/ipv6-header.h"
#include "ns3/icmpv6-header.h"

#include "radvd.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE ("RadvdApplication");

NS_OBJECT_ENSURE_REGISTERED (Radvd);

TypeId Radvd::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::Radvd")
    .SetParent<Application> ()
    .AddConstructor<Radvd> ()
  ;
  return tid;
}
 
Radvd::Radvd ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

Radvd::~Radvd ()
{
  NS_LOG_FUNCTION_NOARGS ();
  for (RadvdInterfaceListI it = m_configurations.begin () ; it != m_configurations.end () ; ++it)
  {
    *it = 0;
  }
  m_configurations.clear ();
  m_socket = 0;
}

void Radvd::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  Application::DoDispose ();
}

void Radvd::StartApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();

  if (!m_socket)
  {
    TypeId tid = TypeId::LookupByName ("ns3::Ipv6RawSocketFactory");
    m_socket = Socket::CreateSocket (GetNode (), tid);

    NS_ASSERT (m_socket);

/*    m_socket->Bind (Inet6SocketAddress (m_localAddress, 0)); */
/*    m_socket->Connect (Inet6SocketAddress (Ipv6Address::GetAllNodesMulticast (), 0)); */
    m_socket->SetAttribute ("Protocol", UintegerValue (58)); /* ICMPv6 */
    m_socket->SetRecvCallback (MakeCallback (&Radvd::HandleRead, this));
  }

  for (RadvdInterfaceListCI it = m_configurations.begin () ; it != m_configurations.end () ; it++)
  {
    m_eventIds[(*it)->GetInterface ()] = EventId ();
    ScheduleTransmit (Seconds (0.), (*it), m_eventIds[(*it)->GetInterface ()]);
  }
}

void Radvd::StopApplication ()
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_socket)
  {
    m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket> > ());
  }

  for (EventIdMapI it = m_eventIds.begin () ; it != m_eventIds.end () ; ++it)
  {
    Simulator::Cancel ((*it).second);
  }
  m_eventIds.clear ();
}

void Radvd::AddConfiguration (Ptr<RadvdInterface> routerInterface)
{
  m_configurations.push_back (routerInterface);
}

void Radvd::ScheduleTransmit (Time dt, Ptr<RadvdInterface> config, EventId& eventId)
{
  NS_LOG_FUNCTION (this << dt);
  eventId = Simulator::Schedule (dt, &Radvd::Send, this, config, Ipv6Address::GetAllNodesMulticast ());
}

void Radvd::Send (Ptr<RadvdInterface> config, Ipv6Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  NS_ASSERT (m_eventIds[config->GetInterface ()].IsExpired ());
  Icmpv6RA raHdr;
  Icmpv6OptionLinkLayerAddress llaHdr;
  Icmpv6OptionMtu mtuHdr;
  Icmpv6OptionPrefixInformation prefixHdr;

  if (m_eventIds.size () == 0)
  {
    return;
  }

  std::list<Ptr<RadvdPrefix> > prefixes = config->GetPrefixes ();
  Ptr<Packet> p = Create<Packet> ();
  Ptr<Ipv6> ipv6 = GetNode ()->GetObject<Ipv6> ();

  /* set RA header information */
  raHdr.SetFlagM (config->IsManagedFlag ());
  raHdr.SetFlagO (config->IsOtherConfigFlag ());
  raHdr.SetFlagH (config->IsHomeAgentFlag ());
  raHdr.SetCurHopLimit (config->GetCurHopLimit ());
  raHdr.SetLifeTime (config->GetDefaultLifeTime ());
  raHdr.SetReachableTime (config->GetReachableTime ());
  raHdr.SetRetransmissionTime (config->GetRetransTimer ());
    
  if (config->IsSourceLLAddress ())
  {
    /* Get L2 address from NetDevice */
    Address addr = ipv6->GetNetDevice (config->GetInterface ())->GetAddress ();
    llaHdr = Icmpv6OptionLinkLayerAddress (true, addr);
    p->AddHeader (llaHdr);
  }

  if (config->GetLinkMtu ())
  {
    NS_ASSERT (config->GetLinkMtu () >= 1280);
    mtuHdr = Icmpv6OptionMtu (config->GetLinkMtu ());
    p->AddHeader (mtuHdr);
  }

  /* add list of prefixes */
  for (std::list<Ptr<RadvdPrefix> >::const_iterator jt = prefixes.begin () ; jt != prefixes.end () ; jt++)
  {
    uint8_t flags = 0;
    prefixHdr = Icmpv6OptionPrefixInformation ();
    prefixHdr.SetPrefix ((*jt)->GetNetwork ());
    prefixHdr.SetPrefixLength ((*jt)->GetPrefixLength ());
    prefixHdr.SetValidTime ((*jt)->GetValidLifeTime ());
    prefixHdr.SetPreferredTime ((*jt)->GetPreferredLifeTime ());

    if ((*jt)->IsOnLinkFlag ())
    {
       flags += 1 << 7;
    }
    
    if ((*jt)->IsAutonomousFlag ())
    {
      flags += 1 << 6;
    }

    if ((*jt)->IsRouterAddrFlag ())
    {
      flags += 1 << 5;
    }

    prefixHdr.SetFlags (flags);

    p->AddHeader (prefixHdr);
  }

  Ipv6Address src = ipv6->GetAddress (config->GetInterface (), 0).GetAddress ();
  m_socket->Bind (Inet6SocketAddress (src, 0));
  m_socket->Connect (Inet6SocketAddress (dst, 0));

  /* as we know interface index that will be used to send RA and 
   * we always send RA with router's link-local address, we can 
   * calculate checksum here.
   */
  raHdr.CalculatePseudoHeaderChecksum (src, dst, p->GetSize () + raHdr.GetSerializedSize (), 58 /* ICMPv6 */);
  p->AddHeader (raHdr);

  /* send RA */
  NS_LOG_LOGIC ("Send RA");
  m_socket->Send (p, 0);

  UniformVariable rnd;
  uint64_t delay = static_cast<uint64_t> (rnd.GetValue (config->GetMinRtrAdvInterval (), config->GetMaxRtrAdvInterval ()) + 0.5);
  Time t = MilliSeconds (delay);
  ScheduleTransmit (t, config, m_eventIds[config->GetInterface ()]);
}

void Radvd::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet = 0;
  Address from;

  while (packet = socket->RecvFrom (from))
  {
    if (Inet6SocketAddress::IsMatchingType (from))
    {
      Ipv6Header hdr;
      Icmpv6RS rsHdr;
      Inet6SocketAddress address = Inet6SocketAddress::ConvertFrom (from);
      uint64_t delay = 0;
      UniformVariable rnd;
      Time t;

      packet->RemoveHeader (hdr);

      switch (*packet->PeekData ())
      {
        case Icmpv6Header::ICMPV6_ND_ROUTER_SOLICITATION:
          /* send RA in response of a RS */
          packet->RemoveHeader (rsHdr);
          NS_LOG_INFO ("Received ICMPv6 Router Solicitation from " << hdr.GetSourceAddress () << " code = " << (uint32_t)rsHdr.GetCode ());

          delay = static_cast<uint64_t> (rnd.GetValue (0, MAX_RA_DELAY_TIME) + 0.5); 
          t = Simulator::Now () + MilliSeconds (delay);

#if 0
          NS_LOG_INFO ("schedule new RA : " << t.GetTimeStep () << " next scheduled RA" << (int64_t)m_sendEvent.GetTs ());

          if (t.GetTimeStep () < static_cast<int64_t> (m_sendEvent.GetTs ()))
          {
            /* send multicast RA */
            /* maybe replace this by a unicast RA (it is a SHOULD in the RFC) */
            NS_LOG_INFO ("Respond to RS");
            /* XXX advertise just the prefix for the interface not all */
            t = MilliSeconds (delay);
            /* XXX schedule packet send */
            /* ScheduleTransmit (t); */
          }
#endif
          break;
        default:
          break;
      }
    }
  }
}

} /* namespace ns3 */

