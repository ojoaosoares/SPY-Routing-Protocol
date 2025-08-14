/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 IITP RAS
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
 * Authors: António Fonseca <afonseca@tagus.inesc-id.pt>, written after OlsrHelper by Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "spy-helper.h"
#include "ns3/spy.h"
#include "ns3/node-list.h"
#include "ns3/names.h"
#include "ns3/ipv4-list-routing.h"
#include "ns3/node-container.h"
#include "ns3/callback.h"
#include "ns3/udp-l4-protocol.h"

namespace ns3 {

SpyHelper::SpyHelper ()
  : Ipv4RoutingHelper ()
{
  m_agentFactory.SetTypeId ("ns3::spy::RoutingProtocol");
}

SpyHelper*
SpyHelper::Copy (void) const
{
  return new SpyHelper (*this);
}

Ptr<Ipv4RoutingProtocol>
SpyHelper::Create (Ptr<Node> node) const
{
  //Ptr<Ipv4L4Protocol> ipv4l4 = node->GetObject<Ipv4L4Protocol> ();
  Ptr<spy::RoutingProtocol> spy = m_agentFactory.Create<spy::RoutingProtocol> ();
  //spy->SetDownTarget (ipv4l4->GetDownTarget ());
  //ipv4l4->SetDownTarget (MakeCallback (&spy::RoutingProtocol::AddHeaders, spy));
  node->AggregateObject (spy);
  return spy;
}

void
SpyHelper::Set (std::string name, const AttributeValue &value)
{
  m_agentFactory.Set (name, value);
}


void 
SpyHelper::Install (void) const
{
  NodeContainer c = NodeContainer::GetGlobal ();
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      Ptr<Node> node = (*i);
      Ptr<UdpL4Protocol> udp = node->GetObject<UdpL4Protocol> ();
      Ptr<spy::RoutingProtocol> spy = node->GetObject<spy::RoutingProtocol> ();
      spy->SetDownTarget (udp->GetDownTarget ());
      udp->SetDownTarget (MakeCallback(&spy::RoutingProtocol::AddHeaders, spy));
    }


}


}
