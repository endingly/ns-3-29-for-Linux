/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008,2009 IITP RAS
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
 * Authors: Kirill Andreev <andreev@iitp.ru>
 *          Aleksey Kovalenko <kovalenko@iitp.ru>
 */


#include "peer-manager-protocol.h"

#include "ns3/dot11s-parameters.h"
#include "ns3/simulator.h"
#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/random-variable.h"
#include "ns3/mesh-wifi-interface-mac.h"

NS_LOG_COMPONENT_DEFINE ("Dot11sPeerManagerProtocol");
namespace ns3 {
/***************************************************
 * PeerLinkDescriptor
 ***************************************************/
WifiPeerLinkDescriptor::WifiPeerLinkDescriptor ():
    m_localLinkId (0),
    m_peerLinkId (0),
    m_state (IDLE),
    m_retryCounter (0),
    m_maxBeaconLoss (3)
{}

void
WifiPeerLinkDescriptor::SetPeerAddress (Mac48Address macaddr)
{
  m_peerAddress = macaddr;
}

void
WifiPeerLinkDescriptor::SetLocalAddress (Mac48Address macaddr)
{
  m_localAddress = macaddr;
}

void
WifiPeerLinkDescriptor::SetLocalLinkId (uint16_t id)
{
  m_localLinkId = id;
}
void
WifiPeerLinkDescriptor::SetLocalAid (uint16_t aid)
{
  m_assocId = aid;
}
void
WifiPeerLinkDescriptor::SetBeaconInformation (Time lastBeacon, Time beaconInterval)
{
  m_lastBeacon = lastBeacon;
  m_beaconInterval = beaconInterval;
  m_beaconLossTimer.Cancel ();
  Time delay = Seconds (beaconInterval.GetSeconds()*m_maxBeaconLoss);
  NS_ASSERT (delay.GetMicroSeconds() != 0);
  m_beaconLossTimer = Simulator::Schedule (delay, &WifiPeerLinkDescriptor::BeaconLoss, this);
}

void
WifiPeerLinkDescriptor::SetMaxBeaconLoss (uint8_t maxBeaconLoss)
{
  m_maxBeaconLoss = maxBeaconLoss;
}

void
WifiPeerLinkDescriptor::SetLinkStatusCallback (Callback<void, Mac48Address, Mac48Address, bool> cb)
{
  m_linkStatusCallback = cb;
}
void
WifiPeerLinkDescriptor::BeaconLoss ()
{
  StateMachine (CNCL);
}

void
WifiPeerLinkDescriptor::SetBeaconTimingElement (IeDot11sBeaconTiming beaconTiming)
{
  m_beaconTiming = beaconTiming;
}

Mac48Address
WifiPeerLinkDescriptor::GetPeerAddress () const
{
  return m_peerAddress;
}

Mac48Address
WifiPeerLinkDescriptor::GetLocalAddress () const
{
  return m_localAddress;
}


uint16_t
WifiPeerLinkDescriptor::GetLocalAid () const
{
  return m_assocId;
}

Time
WifiPeerLinkDescriptor::GetLastBeacon () const
{
  return m_lastBeacon;
}

Time
WifiPeerLinkDescriptor::GetBeaconInterval () const
{
  return m_beaconInterval;
}
IeDot11sBeaconTiming
WifiPeerLinkDescriptor::GetBeaconTimingElement () const
{
  return m_beaconTiming;
}

void
WifiPeerLinkDescriptor::ClearTimingElement ()
{
  m_beaconTiming.ClearTimingElement ();
}


void  WifiPeerLinkDescriptor::MLMECancelPeerLink (dot11sReasonCode reason)
{
  StateMachine (CNCL,reason);
}

void  WifiPeerLinkDescriptor::MLMEPassivePeerLinkOpen ()
{
  StateMachine (PASOPN);
}

void  WifiPeerLinkDescriptor::MLMEActivePeerLinkOpen ()
{
  StateMachine (ACTOPN);
}
void WifiPeerLinkDescriptor::MLMEPeeringRequestReject ()
{
  StateMachine (REQ_RJCT, REASON11S_PEER_LINK_CANCELLED);
}

void WifiPeerLinkDescriptor::PeerLinkClose (uint16_t localLinkId,uint16_t peerLinkId, dot11sReasonCode reason)
{
  if (peerLinkId != 0 && m_localLinkId != peerLinkId)
    return;
  if (m_peerLinkId == 0)
    m_peerLinkId = localLinkId;
  else if (m_peerLinkId != localLinkId)
    return;
  StateMachine (CLS_ACPT, reason);
}

void WifiPeerLinkDescriptor::PeerLinkOpenAccept (uint16_t localLinkId, IeDot11sConfiguration  conf)
{
  if (m_peerLinkId == 0)
    m_peerLinkId = localLinkId;
  m_configuration = conf;
  StateMachine (OPN_ACPT);
}

void WifiPeerLinkDescriptor::PeerLinkOpenReject (uint16_t localLinkId, IeDot11sConfiguration  conf,dot11sReasonCode reason)
{
  if ( m_peerLinkId == 0)
    m_peerLinkId = localLinkId;
  m_configuration = conf;
  StateMachine (OPN_RJCT, reason);
}

void
WifiPeerLinkDescriptor::PeerLinkConfirmAccept (uint16_t localLinkId,uint16_t peerLinkId, uint16_t peerAid, IeDot11sConfiguration  conf)
{
  if ( m_localLinkId != peerLinkId)
    return;
  if ( m_peerLinkId == 0)
    m_peerLinkId = localLinkId;
  else if ( m_peerLinkId != localLinkId )
    return;
  m_configuration = conf;
  m_peerAssocId = peerAid;
  StateMachine (CNF_ACPT);
}

void   WifiPeerLinkDescriptor:: PeerLinkConfirmReject (uint16_t localLinkId, uint16_t peerLinkId,
    IeDot11sConfiguration  conf,dot11sReasonCode reason)
{
  if (m_localLinkId != peerLinkId)
    return;
  if (m_peerLinkId == 0)
    m_peerLinkId = localLinkId;
  else if (m_peerLinkId != localLinkId)
    return;
  m_configuration = conf;
  StateMachine (CNF_RJCT, reason);
}

bool
WifiPeerLinkDescriptor::LinkIsEstab () const
{
  if (m_state == ESTAB)
    return true;
  return false;
}

bool
WifiPeerLinkDescriptor::LinkIsIdle () const
{
  if (m_state == IDLE)
    return true;
  return false;
}

void
WifiPeerLinkDescriptor::StateMachine (PeerEvent event,dot11sReasonCode reasoncode)
{
  switch (m_state)
    {
    case IDLE:
      switch (event)
        {
        case PASOPN:
          m_state = LISTEN;
          break;
        case ACTOPN:
          m_state = OPN_SNT;
          SendPeerLinkOpen ();
          SetRetryTimer ();
          break;
        default:
        {}
        }
      break;
    case LISTEN:
      switch (event)
        {
        case CNCL:
        case CLS_ACPT:
          m_state = IDLE;
          // TODO Callback MLME-SignalPeerLinkStatus
          break;
        case REQ_RJCT:
          SendPeerLinkClose (reasoncode);
          break;
        case ACTOPN:
          m_state = OPN_SNT;
          SendPeerLinkOpen ();
          SetRetryTimer ();
          break;
        case OPN_ACPT:
          m_state = OPN_RCVD;
          SendPeerLinkConfirm ();
          SendPeerLinkOpen ();
          SetRetryTimer ();
          break;
        default:
        {}
        }
      break;
    case OPN_SNT:
      switch (event)
        {
        case TOR1:
          SendPeerLinkOpen ();
          m_retryCounter++;
          SetRetryTimer ();
          break;
        case CNF_ACPT:
          m_state = CNF_RCVD;
          ClearRetryTimer ();
          SetConfirmTimer ();
          break;
        case OPN_ACPT:
          m_state = OPN_RCVD;
          SendPeerLinkConfirm ();
          break;
        case CLS_ACPT:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_MESH_CLOSE_RCVD);
          SetHoldingTimer ();
          break;
        case OPN_RJCT:
        case CNF_RJCT:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (reasoncode);
          SetHoldingTimer ();
          break;
        case TOR2:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_MESH_MAX_RETRIES);
          SetHoldingTimer ();
          break;
        case CNCL:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_PEER_LINK_CANCELLED);
          SetHoldingTimer ();
          break;
        default:
        {}
        }
      break;
    case CNF_RCVD:
      switch (event)
        {
        case CNF_ACPT:
          break;
        case OPN_ACPT:
          m_state = ESTAB;
          NS_LOG_DEBUG ("I am "<<m_localAddress<<", established link with "<<m_peerAddress<<", at "<<Simulator::Now());
          ClearConfirmTimer ();
          SendPeerLinkConfirm ();
          m_linkStatusCallback (m_localAddress, m_peerAddress, true);
          // TODO Callback MLME-SignalPeerLinkStatus
          break;
        case CLS_ACPT:
          m_state = HOLDING;
          ClearConfirmTimer ();
          SendPeerLinkClose (REASON11S_MESH_CLOSE_RCVD);
          SetHoldingTimer ();
          break;
        case CNF_RJCT:
        case OPN_RJCT:
          m_state = HOLDING;
          ClearConfirmTimer ();
          SendPeerLinkClose (reasoncode);
          SetHoldingTimer ();
          break;
        case CNCL:
          m_state = HOLDING;
          ClearConfirmTimer ();
          SendPeerLinkClose (REASON11S_PEER_LINK_CANCELLED);
          SetHoldingTimer ();
          break;
        case TOC:
          m_state = HOLDING;
          SendPeerLinkClose (REASON11S_MESH_CONFIRM_TIMEOUT);
          SetHoldingTimer ();
          break;
        default:
        {}
        }
      break;
    case OPN_RCVD:
      switch (event)
        {
        case TOR1:
          SendPeerLinkOpen ();
          m_retryCounter++;
          SetRetryTimer ();
          break;
        case CNF_ACPT:
          NS_LOG_DEBUG ("I am "<<m_localAddress<<", established link with "<<m_peerAddress<<", at "<<Simulator::Now());
          m_state = ESTAB;
          ClearRetryTimer ();
          m_linkStatusCallback (m_localAddress, m_peerAddress, true);
          // TODO Callback MLME-SignalPeerLinkStatus
          break;
        case CLS_ACPT:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_MESH_CLOSE_RCVD);
          SetHoldingTimer ();
          break;
        case OPN_RJCT:
        case CNF_RJCT:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (reasoncode);
          SetHoldingTimer ();
          break;
        case TOR2:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_MESH_MAX_RETRIES);
          SetHoldingTimer ();
          break;
        case CNCL:
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (REASON11S_PEER_LINK_CANCELLED);
          SetHoldingTimer ();
          break;
        default:
        {}
        }
      break;
    case ESTAB:
      switch (event)
        {
#if 0
        case BNDSA:
          m_state = ESTAB;
          // TODO Callback MLME-SignalPeerLinkStatus
          // TODO
          break;
#endif
        case OPN_ACPT:
          SendPeerLinkConfirm ();
          break;
        case CLS_ACPT:
          NS_LOG_DEBUG ("I am "<<m_localAddress<<", CLOSED link with "<<m_peerAddress<<", at "<<Simulator::Now()<<" Close received");
          m_state = HOLDING;
          SendPeerLinkClose (REASON11S_MESH_CLOSE_RCVD);
          SetHoldingTimer ();
          m_linkStatusCallback (m_localAddress, m_peerAddress, false);
          break;
        case OPN_RJCT:
        case CNF_RJCT:
          NS_LOG_DEBUG ("I am "<<m_localAddress<<", CLOSED link with "<<m_peerAddress<<", at "<<Simulator::Now()<<" Rejected open or confirm");
          m_state = HOLDING;
          ClearRetryTimer ();
          SendPeerLinkClose (reasoncode);
          SetHoldingTimer ();
          m_linkStatusCallback (m_localAddress, m_peerAddress, false);
          break;
        case CNCL:
          NS_LOG_DEBUG ("I am "<<m_localAddress<<", CLOSED link with "<<m_peerAddress<<", at "<<Simulator::Now()<<" Link cancelled");
          m_state = HOLDING;
          SendPeerLinkClose (REASON11S_PEER_LINK_CANCELLED);
          SetHoldingTimer ();
          m_linkStatusCallback (m_localAddress, m_peerAddress, false);
          break;
        default:
        {}
        }
      break;
    case HOLDING:
      switch (event)
        {
        case CLS_ACPT:
          ClearHoldingTimer ();
        case TOH:
          m_state = IDLE;
          // TODO Callback MLME-SignalPeerLinkStatus
          break;
        case OPN_ACPT:
        case CNF_ACPT:
          m_state = HOLDING;
          // reason not spec in D2.0
          SendPeerLinkClose (REASON11S_PEER_LINK_CANCELLED);
          break;
        case OPN_RJCT:
        case CNF_RJCT:
          m_state = HOLDING;
          SendPeerLinkClose (reasoncode);
          break;
        default:
        {}
        }
      break;
    }
}

void WifiPeerLinkDescriptor::ClearRetryTimer ()
{
  m_retryTimer.Cancel ();
}

void WifiPeerLinkDescriptor::ClearConfirmTimer ()
{
  m_confirmTimer.Cancel ();
}

void WifiPeerLinkDescriptor::ClearHoldingTimer ()
{
  m_holdingTimer.Cancel ();
}

void WifiPeerLinkDescriptor::SendPeerLinkClose (dot11sReasonCode reasoncode)
{
  IeDot11sPeerManagement peerElement;
  peerElement.SetPeerClose (m_localLinkId, m_peerLinkId, reasoncode);
  //m_mac->SendPeerLinkClose (peerElement,m_peerAddress);
}

void WifiPeerLinkDescriptor::SendPeerLinkOpen ()
{
  IeDot11sPeerManagement peerElement;
  peerElement.SetPeerOpen (m_localLinkId);
  //NS_ASSERT (m_mac != NULL);
  //m_mac->SendPeerLinkOpen (peerElement, m_peerAddress);
}

void WifiPeerLinkDescriptor::SendPeerLinkConfirm ()
{
  IeDot11sPeerManagement peerElement;
  peerElement.SetPeerConfirm (m_localLinkId, m_peerLinkId);
  //m_mac->SendPeerLinkConfirm (peerElement, m_peerAddress, m_assocId);
}

void WifiPeerLinkDescriptor::SetHoldingTimer ()
{
  m_holdingTimer = Simulator::Schedule (dot11sParameters::dot11MeshHoldingTimeout, &WifiPeerLinkDescriptor::HoldingTimeout, this);
}

void WifiPeerLinkDescriptor::HoldingTimeout ()
{
  StateMachine (TOH);
}

void WifiPeerLinkDescriptor::SetRetryTimer ()
{
  m_retryTimer = Simulator::Schedule (dot11sParameters::dot11MeshRetryTimeout, &WifiPeerLinkDescriptor::RetryTimeout, this);
}

void WifiPeerLinkDescriptor::RetryTimeout ()
{
  if ( m_retryCounter < dot11sParameters::dot11MeshMaxRetries)
    StateMachine (TOR1);
  else
    StateMachine (TOR2);
}

void WifiPeerLinkDescriptor::SetConfirmTimer ()
{
  m_confirmTimer = Simulator::Schedule (dot11sParameters::dot11MeshConfirmTimeout, &WifiPeerLinkDescriptor::ConfirmTimeout, this);
}

void WifiPeerLinkDescriptor::ConfirmTimeout ()
{
  StateMachine (TOC);
}

/***************************************************
 * PeerManager
 ***************************************************/
NS_OBJECT_ENSURE_REGISTERED (Dot11sPeerManagerProtocol);

TypeId
Dot11sPeerManagerProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::Dot11sPeerManagerProtocol")
                      .SetParent<Object> ()
                      .AddConstructor<Dot11sPeerManagerProtocol> ();
#if 0
                      //peerLinkCleanupTimeout. This constant is not specified in Draft 2.0
                      .AddAttribute ("PeerLinkCleanupPeriod",
                                     "PeerLinkCleanupPeriod",
                                     TimeValue (MilliSeconds (80)),
                                     MakeTimeAccessor (&Dot11sPeerManagerProtocol::m_peerLinkCleanupPeriod),
                                     MakeTimeChecker ()
                                    )
                      //MaxBeaconLost. This constant is not specified in Draft 2.0
                      .AddAttribute ("MaxBeaconLost", "Max Beacon Lost",
                                     UintegerValue (3),
                                     MakeUintegerAccessor (&Dot11sPeerManagerProtocol::m_maxBeaconLoss),
                                     MakeUintegerChecker<uint8_t> ()
                                    )
                      //maximum number of peer links.
                      .AddAttribute ("MaxNumberOfPeerLinks",
                                     "Maximum number of peer links ",
                                     UintegerValue (32),
                                     MakeUintegerAccessor (&Dot11sPeerManagerProtocol::m_maxNumberOfPeerLinks),
                                     MakeUintegerChecker<uint8_t> ()
                                    );
#endif
  return tid;

}
Dot11sPeerManagerProtocol::Dot11sPeerManagerProtocol ():
  m_lastAssocId(0),
  m_lastLocalLinkId(1)
{
//  m_numberOfActivePeers = 0;
//  // firs peerLinkId is 1, because 0 means "unknown"
//  m_localLinkId = 1;
//  m_cleanupEvent = Simulator::Schedule (m_peerLinkCleanupPeriod, &Dot11sPeerManagerProtocol::PeerCleanup, this);
}
Dot11sPeerManagerProtocol::~Dot11sPeerManagerProtocol ()
{
#if 0
  m_cleanupEvent.Cancel ();
  //TODO: delete a list of descriptors
  for (
    PeerDescriptorsMap::iterator j = m_peerDescriptors.begin ();
    j != m_peerDescriptors.end ();
    j++)
    {
      int to_delete = 0;
      for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = j->second.begin (); i != j->second.end(); i++)
        {
          to_delete ++;
          (*i)->ClearTimingElement ();
          (*i) = 0;
        }
      for (int i = 0; i < to_delete; i ++)
        j->second.pop_back ();
      j->second.clear ();
    }
  m_peerDescriptors.clear ();
#endif
}
//-----------------------------------------------------
//          UNFINISHED
//-----------------------------------------------------
bool
Dot11sPeerManagerProtocol::AttachPorts(std::vector<Ptr<WifiNetDevice> > interfaces)
{
  NS_LOG_UNCOND("Peer manager attach interfaces started!");
  for(std::vector<Ptr<WifiNetDevice> >::iterator i = interfaces.begin(); i != interfaces.end(); i ++)
  {
    MeshWifiInterfaceMac * mac = dynamic_cast<MeshWifiInterfaceMac *> (PeekPointer ((*i)->GetMac ()));
    if (mac == NULL)
      return false;
    Ptr<Dot11sPeerManagerMacPlugin> peerPlugin = Create<Dot11sPeerManagerMacPlugin> ((*i)->GetIfIndex(), this);
    mac->InstallPlugin(peerPlugin);
    m_plugins[(*i)->GetIfIndex()] = peerPlugin;
  }
  return true;
}

Ptr<IeDot11sBeaconTiming>
Dot11sPeerManagerProtocol::SendBeacon(uint32_t interface, Time currentTbtt, Time beaconInterval)
{
  Ptr<IeDot11sBeaconTiming> retval = Create<IeDot11sBeaconTiming> ();
  NS_LOG_UNCOND("I am sending a beacon");
  BeaconInfoMap::iterator i = m_neighbourBeacons.find(interface);
  if(i == m_neighbourBeacons.end())
    return retval;
  for(BeaconInterfaceInfoMap::iterator j = i->second.begin(); j != i->second.end(); j++)
  {
    //check beacon loss and make a timing element
    if(
        (j->second.referenceTbtt.GetMicroSeconds() +
        (j->second.beaconInterval.GetMicroSeconds()* m_maxBeaconLoss))
          <
        Simulator::Now().GetMicroSeconds()
        )
      continue;
    retval->AddNeighboursTimingElementUnit(j->second.aid, j->second.referenceTbtt, j->second.beaconInterval);
  }
  return retval;
}

void
Dot11sPeerManagerProtocol::ReceiveBeacon(
    uint32_t interface,
    bool meshBeacon,
    IeDot11sBeaconTiming timingElement,
    Mac48Address peerAddress,
    Time receivingTime,
    Time beaconInterval)
{
  NS_LOG_UNCOND("Beacon received from "<<peerAddress);
  NS_LOG_UNCOND(timingElement);
  //find beacon entry and write there a new one
  BeaconInfoMap::iterator i = m_neighbourBeacons.find(interface);
  if(i == m_neighbourBeacons.end())
  {
    NS_LOG_UNCOND("First beacon from this interface");
    BeaconInterfaceInfoMap newMap;
    m_neighbourBeacons[interface] = newMap;
  }
  i = m_neighbourBeacons.find(interface);
  BeaconInterfaceInfoMap::iterator j = i->second.find(peerAddress);
  if(j == i->second.end())
  {
    NS_LOG_UNCOND("first beacon from this station");
    BeaconInfo newInfo;
    newInfo.referenceTbtt = receivingTime;
    newInfo.beaconInterval = beaconInterval;
    newInfo.aid = m_lastAssocId++;
    i->second[peerAddress] = newInfo;
  }
  else
  {
    NS_LOG_UNCOND("last beacon was at"<<j->second.referenceTbtt);
    j->second.referenceTbtt = receivingTime;
    NS_LOG_UNCOND("now  beacon is at"<<j->second.referenceTbtt);
    j->second.beaconInterval = beaconInterval;
  }
  if(!meshBeacon)
    return;
  //TODO: PM STATE Machine
}

void
Dot11sPeerManagerProtocol::ReceivePeerLinkFrame(
    uint32_t interface,
    Mac48Address peerAddress,
    uint16_t aid,
    IeDot11sPeerManagement peerManagementElement,
    IeDot11sConfiguration meshConfig
      )
{
}

#if 0
void
Dot11sPeerManagerProtocol::SetSentBeaconTimers (
  Mac48Address interfaceAddress,
  Time ReferenceTbtt,
  Time BeaconInterval
)
{
  BeaconInfoMap::iterator myBeacon = m_myBeaconInfo.find (interfaceAddress);
  NS_ASSERT (myBeacon != m_myBeaconInfo.end());
  myBeacon->second.referenceTbtt = ReferenceTbtt;
  myBeacon->second.beaconInterval = BeaconInterval;

}

void
Dot11sPeerManagerProtocol::SetReceivedBeaconTimers (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress,
  Time lastBeacon,
  Time beaconInterval,
  IeDot11sBeaconTiming beaconTiming
)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    {
      if ((*i)->GetPeerAddress () == peerAddress)
        {
          (*i)->SetBeaconTimingElement (beaconTiming);
          (*i)->SetBeaconInformation (lastBeacon, beaconInterval);
          return;
        }
    }
  Ptr<WifiPeerLinkDescriptor> new_descriptor =
    AddDescriptor (interfaceAddress, peerAddress, Simulator::Now(), beaconInterval);
  new_descriptor->SetBeaconTimingElement (beaconTiming);
}

bool
Dot11sPeerManagerProtocol::AttachPorts (std::vector<Ptr<WifiNetDevice> > interfaces)
{
  NS_ASSERT (interfaces.size() != 0);
  for (std::vector<Ptr<WifiNetDevice> >::iterator i = interfaces.begin (); i != interfaces.end(); i++)
    {
      MeshWifiMac * meshWifiMac = dynamic_cast<MeshWifiMac *> (PeekPointer ((*i)->GetMac ()));
      if (meshWifiMac == NULL)
        return false;
      meshWifiMac->SetPeerLinkManager (this);
      //Add a mac pointer:
      m_macPointers[meshWifiMac->GetAddress ()] = meshWifiMac;
      //Add descriptor array:
      std::vector<Ptr<WifiPeerLinkDescriptor> > descriptors;
      m_peerDescriptors[meshWifiMac->GetAddress ()] = descriptors;
      //Add beacon timers:
      struct BeaconInfo myBeacon;
      m_myBeaconInfo[meshWifiMac->GetAddress ()] = myBeacon;
    }
  return true;
}
void
Dot11sPeerManagerProtocol::AskIfOpenNeeded (Mac48Address interfaceAddress, Mac48Address peerAddress)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      {
        if (ShouldSendOpen (interfaceAddress, peerAddress))
          (*i)->MLMEActivePeerLinkOpen ();
        break;
      }
}

void
Dot11sPeerManagerProtocol::SetOpenReceived (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress,
  IeDot11sPeerManagement peerMan,
  IeDot11sConfiguration conf
)
{
  dot11sReasonCode reasonCode;
  if (!ShouldAcceptOpen (interfaceAddress, peerAddress,reasonCode))
    return;
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      {
        (*i)->PeerLinkOpenAccept (peerMan.GetLocalLinkId(), conf);
        return;
      }
  BeaconInfoMap::iterator myBeacon =  m_myBeaconInfo.find (interfaceAddress);
  NS_ASSERT (myBeacon != m_myBeaconInfo.end());
  Ptr<WifiPeerLinkDescriptor>new_descriptor = AddDescriptor (
        interfaceAddress,
        peerAddress,
        Simulator::Now (),
        myBeacon->second.beaconInterval
      );
  new_descriptor->PeerLinkOpenAccept (peerMan.GetLocalLinkId(), conf);
}
void
Dot11sPeerManagerProtocol::SetConfirmReceived (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress,
  uint16_t peerAid,
  IeDot11sPeerManagement peerMan,
  IeDot11sConfiguration meshConfig
)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      (*i)->PeerLinkConfirmAccept (peerMan.GetLocalLinkId(), peerMan.GetPeerLinkId(), peerAid, meshConfig);
}

void
Dot11sPeerManagerProtocol::SetCloseReceived (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress,
  IeDot11sPeerManagement peerMan
)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      {
        (*i)->PeerLinkClose (peerMan.GetLocalLinkId(), peerMan.GetPeerLinkId(), peerMan.GetReasonCode());
        return;
      }
}

void
Dot11sPeerManagerProtocol::ConfigurationMismatch (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress
)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      {
        (*i)->MLMECancelPeerLink (REASON11S_MESH_CONFIGURATION_POLICY_VIOLATION);
        return;
      }

}

IeDot11sBeaconTiming
Dot11sPeerManagerProtocol::GetIeDot11sBeaconTimingForMyBeacon (Mac48Address interfaceAddress)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  IeDot11sBeaconTiming return_val;
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    {
      //Just go through all neighbor entries and add it to timing element:
      return_val.AddNeighboursTimingElementUnit (
        (*i)->GetLocalAid (),
        (*i)->GetLastBeacon (),
        (*i)->GetBeaconInterval ()
      );
    }
  return return_val;

}
IeDot11sBeaconTiming
Dot11sPeerManagerProtocol::GetIeDot11sBeaconTimingForAddress (
  Mac48Address interfaceAddress,
  Mac48Address addr)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  IeDot11sBeaconTiming return_val;
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == addr)
      return_val =  (*i)->GetBeaconTimingElement ();
  return return_val;
}
Ptr<WifiPeerLinkDescriptor>
Dot11sPeerManagerProtocol::AddDescriptor (
  Mac48Address interfaceAddress,
  Mac48Address peerAddress,
  Time lastBeacon,
  Time beaconInterval)
{
  Ptr<WifiPeerLinkDescriptor> new_descriptor = Create<WifiPeerLinkDescriptor> ();
  if (m_assocId == 0xff)
    m_assocId = 0;
  if (m_localLinkId == 0xff)
    m_localLinkId = 0;
  new_descriptor->SetLocalAid (m_assocId++);
  new_descriptor->SetLocalLinkId (m_localLinkId++);
  new_descriptor->SetPeerAddress (peerAddress);
  new_descriptor->SetBeaconInformation (lastBeacon, beaconInterval);
  //DEBUG ONLY:
  new_descriptor->SetLocalAddress (interfaceAddress);
  //check if interface address is wrong
  MeshMacMap::iterator pos = m_macPointers.find (interfaceAddress);
  NS_ASSERT (pos != m_macPointers.end());
  //check if descriptors array exist
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  new_descriptor->SetMac (pos->second);
  new_descriptor->SetMaxBeaconLoss (m_maxBeaconLoss);
  new_descriptor->SetLinkStatusCallback (MakeCallback(&Dot11sPeerManagerProtocol::PeerLinkStatus, this));
  NS_ASSERT (interface != m_peerDescriptors.end());
  m_peerDescriptors[interfaceAddress].push_back (new_descriptor);
  return new_descriptor;
}

void
Dot11sPeerManagerProtocol::PeerCleanup ()
{
  for (
    PeerDescriptorsMap::iterator j = m_peerDescriptors.begin ();
    j != m_peerDescriptors.end ();
    j++)
    {
      std::vector<unsigned int> to_erase;
      for (unsigned int i = 0; i< j->second.size (); i++)
        if (j->second[i]->LinkIsIdle ())
          {
            j->second[i]->ClearTimingElement ();
            j->second[i] = 0;
            to_erase.push_back (i);
          }
      if (to_erase.size () == 0)
        return;
      for (unsigned int i = to_erase.size ()-1 ; i >= 0; i--)
        j->second.erase (j->second.begin() + to_erase[i]);
      to_erase.clear ();
    }
  m_cleanupEvent = Simulator::Schedule (m_peerLinkCleanupPeriod, &Dot11sPeerManagerProtocol::PeerCleanup, this);
}

std::vector<Mac48Address>
Dot11sPeerManagerProtocol::GetNeighbourAddressList (Mac48Address interfaceAddress, Mac48Address peerAddress)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  std::vector<Mac48Address> return_value;
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    return_value.push_back ((*i)->GetPeerAddress());
  return return_value;
}

bool
Dot11sPeerManagerProtocol::IsActiveLink (Mac48Address interfaceAddress, Mac48Address peerAddress)
{
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    if ((*i)->GetPeerAddress () == peerAddress)
      return ((*i)->LinkIsEstab ());
  return false;
}

bool
Dot11sPeerManagerProtocol::ShouldSendOpen (Mac48Address interfaceAddress, Mac48Address peerAddress)
{
  if (m_numberOfActivePeers > m_maxNumberOfPeerLinks)
    return false;
  return true;
}

bool
Dot11sPeerManagerProtocol::ShouldAcceptOpen (Mac48Address interfaceAddress, Mac48Address peerAddress,dot11sReasonCode & reasonCode)
{
  if (m_numberOfActivePeers > m_maxNumberOfPeerLinks)
    {
      reasonCode = REASON11S_MESH_MAX_PEERS;
      return false;
    }
  return true;
}

Time
Dot11sPeerManagerProtocol::GetNextBeaconShift (
  Mac48Address interfaceAddress,
  Time myNextTBTT
)
{
  //REMINDER:: in timing element  1) last beacon reception time is measured in units of 256 microseconds
  //                              2) beacon interval is mesured in units of 1024 microseconds
  //                              3) hereafter TU = 1024 microseconds
  //Im my MAC everything is stored in MicroSeconds

  uint32_t myNextTBTTInTimeUnits = 0;
  uint32_t futureBeaconInTimeUnits = 0;
  //Going through all my timing elements and detecting future beacon collisions
  PeerDescriptorsMap::iterator interface = m_peerDescriptors.find (interfaceAddress);
  NS_ASSERT (interface != m_peerDescriptors.end());
  BeaconInfoMap::iterator myBeacon = m_myBeaconInfo.find (interfaceAddress);
  NS_ASSERT (myBeacon != m_myBeaconInfo.end());
  for (std::vector<Ptr<WifiPeerLinkDescriptor> >::iterator i = interface->second.begin (); i != interface->second.end(); i++)
    {
      IeDot11sBeaconTiming::NeighboursTimingUnitsList neighbours;
      neighbours = (*i)->GetBeaconTimingElement ().GetNeighboursTimingElementsList();
      //first let's form the list of all kown TBTTs
      for (IeDot11sBeaconTiming::NeighboursTimingUnitsList::const_iterator j = neighbours.begin (); j != neighbours.end(); j++)
        {
          uint16_t beaconIntervalTimeUnits;
          beaconIntervalTimeUnits = (*j)->GetBeaconInterval ();

          //The last beacon time in timing elememt in Time Units
          uint32_t lastBeaconInTimeUnits;
          lastBeaconInTimeUnits = (*j)->GetLastBeacon ()/4;

          //The time of my next beacon sending in Time Units
          myNextTBTTInTimeUnits = myNextTBTT.GetMicroSeconds ()/1024;

          //My beacon interval in Time Units
          uint32_t myBeaconIntervalInTimeUnits;
          myBeaconIntervalInTimeUnits = myBeacon->second.beaconInterval.GetMicroSeconds ()/1024;

          //The time the beacon of other station will be sent
          //we need the time just after my next TBTT (or equal to my TBTT)
          futureBeaconInTimeUnits = lastBeaconInTimeUnits + beaconIntervalTimeUnits;

          //We apply MBAC only if beacon Intervals are equal
          if (beaconIntervalTimeUnits == myBeaconIntervalInTimeUnits)
            {
              //We know when the neighbor STA transmitted it's beacon
              //Now we need to know when it's going to send it's beacon in the future
              //So let's use the valuse of it's beacon interval
              while (myNextTBTTInTimeUnits >= futureBeaconInTimeUnits)
                futureBeaconInTimeUnits = futureBeaconInTimeUnits + beaconIntervalTimeUnits;
              //If we found that my TBTT coincide with another STA's TBTT
              //break all cylce and return time shift for my next TBTT
              if (myNextTBTTInTimeUnits == futureBeaconInTimeUnits)
                break;
            }

        }
      if (myNextTBTTInTimeUnits == futureBeaconInTimeUnits)
        break;
    }

  //TBTTs coincide, so let's calculate the shift
  if (myNextTBTTInTimeUnits == futureBeaconInTimeUnits)
    {
      NS_LOG_DEBUG ("MBCA: Future beacon collision is detected, applying avoidance mechanism");
      UniformVariable randomSign (-1, 1);
      int coefficientSign = -1;
      if (randomSign.GetValue () >= 0)
        coefficientSign = 1;
      UniformVariable randomShift (1, 15);
      //So, the shift is a random integer variable uniformly distributed in [-15;-1] U [1;15]
      int beaconShift = randomShift.GetInteger (1,15) * coefficientSign;
      NS_LOG_DEBUG ("Shift value = " << beaconShift << " beacon TUs");
      //We need the result not in Time Units, but in microseconds
      return MicroSeconds (beaconShift * 1024);
    }
  //No collision detecterf, hence no shift is needed
  else
    return MicroSeconds (0);
}

void
Dot11sPeerManagerProtocol::PeerLinkStatus (Mac48Address interfaceAddress, Mac48Address peerAddress, bool status)
{
  MeshMacMap::iterator pos = m_macPointers.find (interfaceAddress);
  NS_ASSERT (pos != m_macPointers.end());
  pos->second->PeerLinkStatus (peerAddress, status);
}
#endif
} //namespace NS3
