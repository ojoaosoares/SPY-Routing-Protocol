/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * SPY
 *
 */
#define NS_LOG_APPEND_CONTEXT                                           \
  if (m_ipv4) { std::clog << "[node " << m_ipv4->GetObject<Node> ()->GetId () << "] "; }

#include "spy.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/double.h"
#include <algorithm>
#include <limits>
#include "ns3/random-variable-stream.h"

#define SPY_LS_GOD 0

#define SPY_LS_RLS 1
namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("SpyRoutingProtocol");
  namespace spy {
    struct DeferredRouteOutputTag : public Tag
    {
      /// Positive if output device is fixed in RouteOutput
      uint32_t m_isCallFromL3;

      DeferredRouteOutputTag () : Tag (),
                                  m_isCallFromL3 (0)
      {
      }

      static TypeId GetTypeId ()
      {
        static TypeId tid = TypeId ("ns3::spy::DeferredRouteOutputTag").SetParent<Tag> ();
        return tid;
      }

      TypeId  GetInstanceTypeId () const
      {
        return GetTypeId ();
      }

      uint32_t GetSerializedSize () const
      {
        return sizeof(uint32_t);
      }

      void  Serialize (TagBuffer i) const
      {
        i.WriteU32 (m_isCallFromL3);
      }

      void  Deserialize (TagBuffer i)
      {
        m_isCallFromL3 = i.ReadU32 ();
      }

      void  Print (std::ostream &os) const
      {
        os << "DeferredRouteOutputTag: m_isCallFromL3 = " << m_isCallFromL3;
      }
    };



    /********** Miscellaneous constants **********/

    /// Maximum allowed jitter.
    #define SPY_MAXJITTER          (HelloInterval.GetSeconds () / 2)
    /// Random number between [(-SPY_MAXJITTER)-SPY_MAXJITTER] used to jitter HELLO packet transmission.
    #define JITTER (Seconds (UniformRandomVariable ().GetValue (-SPY_MAXJITTER, SPY_MAXJITTER))) 
    #define FIRST_JITTER (Seconds (UniformRandomVariable ().GetValue (0, SPY_MAXJITTER))) //first Hello can not be in the past, used only on SetIpv4



    NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

    /// UDP Port for SPY control traffic, not defined by IANA yet
    const uint32_t RoutingProtocol::SPY_PORT = 666;

    RoutingProtocol::RoutingProtocol ()
      : HelloInterval (Seconds (1)),
        MaxQueueLen (64),
        MaxQueueTime (Seconds (30)),
        m_queue (MaxQueueLen, MaxQueueTime),
        HelloIntervalTimer (Timer::CANCEL_ON_DESTROY),
        PerimeterMode (false)
    {

      m_neighbors = PositionTable ();
    }

    TypeId
    RoutingProtocol::GetTypeId (void)
    {
      static TypeId tid = TypeId ("ns3::spy::RoutingProtocol")
        .SetParent<Ipv4RoutingProtocol> ()
        .AddConstructor<RoutingProtocol> ()
        .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                      TimeValue (Seconds (1)),
                      MakeTimeAccessor (&RoutingProtocol::HelloInterval),
                      MakeTimeChecker ())
        .AddAttribute ("LocationServiceName", "Indicates wich Location Service is enabled",
                      EnumValue (SPY_LS_GOD),
                      MakeEnumAccessor (&RoutingProtocol::LocationServiceName),
                      MakeEnumChecker (SPY_LS_GOD, "GOD",
                                        SPY_LS_RLS, "RLS"))
        .AddAttribute ("PerimeterMode", "Indicates if PerimeterMode is enabled",
                      BooleanValue (false),
                      MakeBooleanAccessor (&RoutingProtocol::PerimeterMode),
                      MakeBooleanChecker ())
      ;
      return tid;
    }

    RoutingProtocol::~RoutingProtocol ()
    {
    }

    void
    RoutingProtocol::DoDispose ()
    {
      m_ipv4 = 0;
      Ipv4RoutingProtocol::DoDispose ();
    }

    Ptr<LocationService>
    RoutingProtocol::GetLS ()
    {
      return m_locationService;
    }
    void
    RoutingProtocol::SetLS (Ptr<LocationService> locationService)
    {
      m_locationService = locationService;
    }


    bool RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                                      UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                                      LocalDeliverCallback lcb, ErrorCallback ecb)
    {

    NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestination () << idev->GetAddress ());
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No spy interfaces");
          return false;
        }
      NS_ASSERT (m_ipv4 != 0);
      NS_ASSERT (p != 0);
      // Check if input device supports IP
      NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
      int32_t iif = m_ipv4->GetInterfaceForDevice (idev);
      Ipv4Address dst = header.GetDestination ();
      Ipv4Address origin = header.GetSource ();

      DeferredRouteOutputTag tag; //FIXME since I have to check if it's in origin for it to work it means I'm not taking some tag out...
      if (p->PeekPacketTag (tag) && IsMyOwnAddress (origin))
        {
          Ptr<Packet> packet = p->Copy (); //FIXME ja estou a abusar de tirar tags
          packet->RemovePacketTag(tag);
          DeferredRouteOutput (packet, header, ucb, ecb);
          return true; 
        }



      if (m_ipv4->IsDestinationAddress (dst, iif))
        {

          Ptr<Packet> packet = p->Copy ();
          TypeHeader tHeader (SPY_TYPE_POS);
          packet->RemoveHeader (tHeader);
          if (!tHeader.IsValid ())
            {
              NS_LOG_DEBUG ("SPY message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Ignored");
              return false;
            }
          
          DisjointHeader disHdr;
          if (tHeader.Get () == SPY_TYPE_POS)
          {
              PositionHeader phdr;
              packet->RemoveHeader (phdr);
            
              packet->RemoveHeader(disHdr);
          }

          Ipv4Address lastHop = disHdr.GetLastHop();

          PacketKey inverse = std::make_tuple(origin, dst, disHdr.GetPathId() == 0 ? 1 : 0);
          m_neighbors.AddNotForward(inverse, lastHop);

          AddParityPath(origin, disHdr.GetPathId(), disHdr.GetParity());
          
          if (!CheckParityPathsTimer.IsRunning())
          {
              CheckParityPathsTimer.Schedule(Seconds(1));
          }
      
          if (dst != m_ipv4->GetAddress (1, 0).GetBroadcast ())
            {
              NS_LOG_LOGIC ("Unicast local delivery to " << dst);
            }
          else
            {
    //          NS_LOG_LOGIC ("Broadcast local delivery to " << dst);
            }

          lcb (packet, header, iif);
          return true;
        }


      return Forwarding (p, header, ucb, ecb);
    }


    void
    RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header,
                                          UnicastForwardCallback ucb, ErrorCallback ecb)
    {
      NS_LOG_FUNCTION (this << p << header);
      NS_ASSERT (p != 0 && p != Ptr<Packet> ());

      if (m_queue.GetSize () == 0)
        {
          CheckQueueTimer.Cancel ();
          CheckQueueTimer.Schedule (Time ("500ms"));
        }

      QueueEntry newEntry (p, header, ucb, ecb);
      bool result = m_queue.Enqueue (newEntry);


      m_queuedAddresses.insert (m_queuedAddresses.begin (), header.GetDestination ());
      m_queuedAddresses.unique ();

      if (result)
        {
          NS_LOG_LOGIC ("Add packet " << p->GetUid () << " to queue. Protocol " << (uint16_t) header.GetProtocol ());

        }

    }

    void
    RoutingProtocol::CheckQueue ()
    {

      CheckQueueTimer.Cancel ();

      std::list<Ipv4Address> toRemove;

      for (std::list<Ipv4Address>::iterator i = m_queuedAddresses.begin (); i != m_queuedAddresses.end (); ++i)
        {
          if (SendPacketFromQueue (*i))
            {
              //Insert in a list to remove later
              toRemove.insert (toRemove.begin (), *i);
            }
        }

      //remove all that are on the list
      for (std::list<Ipv4Address>::iterator i = toRemove.begin (); i != toRemove.end (); ++i)
        {
          m_queuedAddresses.remove (*i);
        }

      if (!m_queuedAddresses.empty ()) //Only need to schedule if the queue is not empty
        {
          CheckQueueTimer.Schedule (Time ("500ms"));
        }
    }

    bool
    RoutingProtocol::SendPacketFromQueue (Ipv4Address dst)
    {
        NS_LOG_FUNCTION (this);
        bool recovery = false;
        QueueEntry queueEntry;

        if (m_locationService->IsInSearch (dst))
        {
            return false;
        }

        if (!m_locationService->HasPosition (dst)) // Location-service stoped looking for the dst
        {
            m_queue.DropPacketWithDst (dst);
            NS_LOG_LOGIC ("Location Service did not find dst. Drop packet to " << dst);
            return true;
        }

        Vector myPos;
        Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
        myPos.x = MM->GetPosition ().x;
        myPos.y = MM->GetPosition ().y;

        Ipv4Address nextHop;

        Vector Position;
        Vector previousHop;

        uint16_t positionX = 0;
        uint16_t positionY = 0;
        uint32_t hdrTime = 0;

        if(dst != m_ipv4->GetAddress (1, 0).GetBroadcast ())
        {
            positionX = m_locationService->GetPosition (dst).x;
            positionY = m_locationService->GetPosition (dst).y;
            hdrTime = (uint32_t) m_locationService->GetEntryUpdateTime (dst).GetSeconds ();
        }

        PositionHeader posGreedyHeader (positionX, positionY,  hdrTime, (uint64_t) 0,(uint64_t) 0, (uint8_t) 0, myPos.x, myPos.y); 

        PositionHeader posRecHeader (positionX, positionY,  hdrTime, myPos.x, myPos.y, (uint8_t) 1, positionX, positionY); 

        while(m_queue.Dequeue (dst, queueEntry))
        {
          Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());

          UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
          Ipv4Header header = queueEntry.GetIpv4Header ();
          
          TypeHeader tHeader (SPY_TYPE_POS);
          p->RemoveHeader (tHeader);

          if (!tHeader.IsValid ())
          {
            NS_LOG_DEBUG ("SPY message " << p->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
            return false;     // drop
          }

          DisjointHeader disHdr;

          if (tHeader.Get () == SPY_TYPE_POS)
          {
              PositionHeader hdr;
              p->RemoveHeader (hdr);
              p->RemoveHeader(disHdr);
          }

          else 
            return false;

          PacketKey key = std::make_tuple(m_ipv4->GetAddress(1, 0).GetLocal(), dst, disHdr.GetPathId());

          if(m_neighbors.isNeighbour (key, dst))
          {
              nextHop = dst;
          }

          else
          {
              Vector dstPos = m_locationService->GetPosition (dst);
              nextHop = m_neighbors.BestNeighbor (key, dstPos, myPos);

              if (nextHop == Ipv4Address::GetZero ())
              {
                NS_LOG_LOGIC ("Fallback to recovery-mode. Packets to " << dst);
                recovery = true;
              }

              if(recovery)
              {

                p->AddHeader(disHdr);
                p->AddHeader(posRecHeader); //enters in recovery with last edge from Dst
                p->AddHeader(tHeader);
                
                RecoveryMode(dst, p, ucb, header);

                continue;
              }
          } 

          Ptr<Ipv4Route> route = Create<Ipv4Route> ();
          route->SetDestination (dst);
          route->SetGateway (nextHop);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));

          p->AddHeader(disHdr);
          p->AddHeader(posGreedyHeader); //enters in recovery with last edge from Dst
          p->AddHeader(tHeader);

          if (header.GetSource () == Ipv4Address ("102.102.102.102"))
          {
            route->SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
            header.SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
          }

          else
          {
            route->SetSource (header.GetSource ());
          }

          ucb (route, p, header);
        }

        return true;
    }

    void 
    RoutingProtocol::RecoveryMode(Ipv4Address dst, Ptr<Packet> p, UnicastForwardCallback ucb, Ipv4Header header){

      Vector Position;
      Vector previousHop;
      uint32_t updated;
      uint64_t positionX;
      uint64_t positionY;
      Vector myPos;
      Vector recPos;

      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
      positionX = MM->GetPosition ().x;
      positionY = MM->GetPosition ().y;
      myPos.x = positionX;
      myPos.y = positionY;  

      TypeHeader tHeader (SPY_TYPE_POS);
      p->RemoveHeader (tHeader);
      if (!tHeader.IsValid ())
        {
          NS_LOG_DEBUG ("SPY message " << p->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
          return;     // drop
        }

      DisjointHeader disHdr;

      if (tHeader.Get () == SPY_TYPE_POS)
        {
          PositionHeader hdr;
          p->RemoveHeader (hdr);
          Position.x = hdr.GetDstPosx ();
          Position.y = hdr.GetDstPosy ();
          updated = hdr.GetUpdated (); 
          recPos.x = hdr.GetRecPosx ();
          recPos.y = hdr.GetRecPosy ();
          previousHop.x = hdr.GetLastPosx ();
          previousHop.y = hdr.GetLastPosy ();
          
          p->RemoveHeader(disHdr);
      }

      PacketKey key = std::make_tuple(header.GetSource(), dst, disHdr.GetPathId());

      PositionHeader posHeader (Position.x, Position.y,  updated, recPos.x, recPos.y, (uint8_t) 1, myPos.x, myPos.y); 

      p->AddHeader(disHdr);
      p->AddHeader (posHeader);
      p->AddHeader (tHeader);

      Ipv4Address nextHop = m_neighbors.BestAngle (key, previousHop, myPos); 
      if (nextHop == Ipv4Address::GetZero ())
      {
        return;
      }

      Ptr<Ipv4Route> route = Create<Ipv4Route> ();
      route->SetDestination (dst);
      route->SetGateway (nextHop);

      // FIXME: Does not work for multiple interfaces
      route->SetOutputDevice (m_ipv4->GetNetDevice (1));
      route->SetSource (header.GetSource ());

      NS_LOG_LOGIC (route->GetOutputDevice () << " forwarding in Recovery to " << dst << " through " << route->GetGateway () << " packet " << p->GetUid ());
      ucb (route, p, header);
      
      return;
    }


    void
    RoutingProtocol::NotifyInterfaceUp (uint32_t interface)
    {
      NS_LOG_FUNCTION (this << m_ipv4->GetAddress (interface, 0).GetLocal ());
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (interface) > 1)
        {
          NS_LOG_WARN ("SPY does not work with more then one address per each interface.");
        }
      Ipv4InterfaceAddress iface = l3->GetAddress (interface, 0);
      if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
        {
          return;
        }

      // Create a socket to listen only on this interface
      Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                UdpSocketFactory::GetTypeId ());
      NS_ASSERT (socket != 0);
      socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvSPY, this));
      socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), SPY_PORT));
      socket->BindToNetDevice (l3->GetNetDevice (interface));
      socket->SetAllowBroadcast (true);
      socket->SetAttribute ("IpTtl", UintegerValue (1));
      m_socketAddresses.insert (std::make_pair (socket, iface));


      // Allow neighbor manager use this interface for layer 2 feedback if possible
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
      Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
      if (wifi == 0)
        {
          return;
        }
      Ptr<WifiMac> mac = wifi->GetMac ();
      if (mac == 0)
        {
          return;
        }

      mac->TraceConnectWithoutContext ("TxErrHeader", m_neighbors.GetTxErrorCallback ());

    }


    void
    RoutingProtocol::RecvSPY (Ptr<Socket> socket)
    {
      NS_LOG_FUNCTION (this << socket);
      Address sourceAddress;
      Ptr<Packet> packet = socket->RecvFrom (sourceAddress);

      TypeHeader tHeader (SPY_TYPE_HELLO);
      packet->RemoveHeader (tHeader);
      if (!tHeader.IsValid ())
      {
          NS_LOG_DEBUG ("SPY message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Ignored");
          return;
      }

      if (tHeader.Get() == SPY_TYPE_TAKE_SHORTCUT)
      {
          PathId piHdr;
          TakeShortcut tsHdr;

          packet->RemoveHeader(piHdr);
          packet->RemoveHeader(tsHdr);

          PacketKey inversekey = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId() == 0 ? 1 : 0);

          if (m_neighbors.isNeighbour(inversekey, tsHdr.GetShortcut()))
          {
              PacketKey key = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId());

              m_neighbors.AddNotSend(key, tsHdr.GetShortcut());

              return;
          }

          tHeader.Set(SPY_TYPE_NEIGH_INTERSECTION);

          packet->AddHeader(piHdr);
          
      }

      if (tHeader.Get() == SPY_TYPE_HELLO)
      {

        HelloHeader hdr;
        packet->RemoveHeader (hdr);
        Vector Position;
        Position.x = hdr.GetOriginPosx ();
        Position.y = hdr.GetOriginPosy ();
        InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
        Ipv4Address sender = inetSourceAddr.GetIpv4 ();
        Ipv4Address receiver = m_socketAddresses[socket].GetLocal ();

        UpdateRouteToNeighbor (sender, receiver, Position);
      }

      else if (tHeader.Get() == SPY_TYPE_IN_ANALYSIS)
      {
        PathId piHdr;
        NeighIntersection niHdr;
        TakeShortcut tsHdr;

        packet->RemoveHeader(piHdr);
        packet->RemoveHeader(niHdr);
        packet->RemoveHeader(tsHdr);

        if (m_ipv4->GetAddress(1, 0).GetLocal() != niHdr.GetSource())
        {
          PacketKey inversekey = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId() == 0 ? 1 : 0);

          if (!m_neighbors.HasNotForward(inversekey) && m_neighbors.isNeighbour(inversekey, niHdr.GetDest()))
          {
              PacketKey key = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId());

              m_neighbors.AddNotForward(key, niHdr.GetSource());

              m_neighbors.AddNotSend(key, niHdr.GetDest());

              TakeShortcut newTsHdr(m_ipv4->GetAddress(1, 0).GetLocal());
              TypeHeader newThdr(SPY_TYPE_SET_PATH);

              for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
              {
                Ptr<Socket> socket = j->first;

                Ptr<Packet> newpacket = Create<Packet>();

                newpacket->AddHeader(newTsHdr);
                newpacket->AddHeader(piHdr);
                newpacket->AddHeader(newThdr);
                
                Ipv4Address destination = niHdr.GetSource();
                
                socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
              }
          }

          else
          {
              for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
              {
                Ptr<Socket> socket = j->first;

                Ptr<Packet> newpacket = Create<Packet>();

                newpacket->AddHeader(tsHdr);
                newpacket->AddHeader(niHdr);
                newpacket->AddHeader(piHdr);
                newpacket->AddHeader(tHeader);
                
                Ipv4Address destination = niHdr.GetSource();
                
                socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
              }
          }
        }
      
        else
        {
            std::vector<Ipv4Address> inter = niHdr.GetNeighbors();

            if (!inter.empty())
            { 
                Ipv4Address nextHop = niHdr.PopLastNeighbor();

                for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
                {
                  Ptr<Socket> socket = j->first;

                  Ptr<Packet> newpacket = Create<Packet>();

                  newpacket->AddHeader(tsHdr);
                  newpacket->AddHeader(niHdr);
                  newpacket->AddHeader(piHdr);
                  newpacket->AddHeader(tHeader);
                  
                  Ipv4Address destination = nextHop;
                  
                  socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
                }
            }

            else
            {
                PacketKey key = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId());
                Ipv4Address lastHop = m_neighbors.getLastSender(key);

                if (lastHop != Ipv4Address::GetZero())
                {
                    TakeShortcut newtsHdr(m_ipv4->GetAddress(1, 0).GetLocal());
                    NeighIntersection newneighHdr(lastHop, m_ipv4->GetAddress(1, 0).GetLocal());

                    TypeHeader newtHdr(SPY_TYPE_TAKE_SHORTCUT);

                    std::map<Ipv4Address, std::pair<Vector, Time>> table = m_neighbors.GetNeighborTable();

                    for (const auto& entry : table)
                    {
                      newneighHdr.AddNeighbor(entry.first);
                    } 

                    for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
                    {
                      Ptr<Socket> socket = j->first;

                      Ptr<Packet> newpacket = Create<Packet>();

                      newpacket->AddHeader(newtsHdr);
                      newpacket->AddHeader(newneighHdr);
                      newpacket->AddHeader(tsHdr);
                      newpacket->AddHeader(piHdr);
                      newpacket->AddHeader(newtHdr);
                      
                      Ipv4Address destination = lastHop;
                      
                      socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
                    }
                }

            }
        }
      }

      else if (tHeader.Get() == SPY_TYPE_NEIGH_INTERSECTION)
      {
        PathId piHdr;
        NeighIntersection niHdr;
        TakeShortcut tsHdr;

        packet->RemoveHeader(piHdr);
        packet->RemoveHeader(niHdr);
        packet->RemoveHeader(tsHdr);

        std::vector<Ipv4Address> inter = m_neighbors.GetIntersection(niHdr.GetNeighbors());

        if (!inter.empty())
        {
            NeighIntersection intersectionHdr(niHdr.GetSource(), niHdr.GetDest());
            intersectionHdr.SetNeighbors(inter);

            Ipv4Address nextHop = intersectionHdr.PopLastNeighbor();

            TypeHeader tHdr(SPY_TYPE_IN_ANALYSIS);

            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
            {
              Ptr<Socket> socket = j->first;

              Ptr<Packet> newpacket = Create<Packet>();

              newpacket->AddHeader(tsHdr);
              newpacket->AddHeader(intersectionHdr);
              newpacket->AddHeader(piHdr);
              newpacket->AddHeader(tHdr);
              
              Ipv4Address destination = nextHop;
              
              socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
            }
        }

        else
        {
            PacketKey key = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId());
            Ipv4Address lastHop = m_neighbors.getLastSender(key);

            if (lastHop != Ipv4Address::GetZero())
            {
                TakeShortcut newtsHdr(m_ipv4->GetAddress(1, 0).GetLocal());
                NeighIntersection newneighHdr(lastHop, m_ipv4->GetAddress(1, 0).GetLocal());

                TypeHeader newtHdr(SPY_TYPE_TAKE_SHORTCUT);

                std::map<Ipv4Address, std::pair<Vector, Time>> table = m_neighbors.GetNeighborTable();

                for (const auto& entry : table)
                {
                  newneighHdr.AddNeighbor(entry.first);
                } 

                for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
                {
                  Ptr<Socket> socket = j->first;

                  Ptr<Packet> newpacket = Create<Packet>();

                  newpacket->AddHeader(newtsHdr);
                  newpacket->AddHeader(newneighHdr);
                  newpacket->AddHeader(tsHdr);
                  newpacket->AddHeader(piHdr);
                  newpacket->AddHeader(newtHdr);
                  
                  Ipv4Address destination = lastHop;
                  
                  socket->SendTo (newpacket, 0, InetSocketAddress (destination, SPY_PORT));
                }
            }

        }
      }

      else if (tHeader.Get() == SPY_TYPE_SET_PATH)
      {
          PathId piHdr;
          TakeShortcut tsHdr;
          packet->RemoveHeader(piHdr);          
          packet->RemoveHeader(tsHdr);

          PacketKey key = std::make_tuple(piHdr.GetSource(), piHdr.GetDest(), piHdr.GetId());

          m_neighbors.AddNotSend(key, tsHdr.GetShortcut());
      }
    }


    void
    RoutingProtocol::UpdateRouteToNeighbor (Ipv4Address sender, Ipv4Address receiver, Vector Pos)
    {
      m_neighbors.AddEntry (sender, Pos);

    }



    void
    RoutingProtocol::NotifyInterfaceDown (uint32_t interface)
    {
      NS_LOG_FUNCTION (this << m_ipv4->GetAddress (interface, 0).GetLocal ());

      // Disable layer 2 link state monitoring (if possible)
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      Ptr<NetDevice> dev = l3->GetNetDevice (interface);
      Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
      if (wifi != 0)
        {
          Ptr<WifiMac> mac = wifi->GetMac ()->GetObject<AdhocWifiMac> ();
          if (mac != 0)
            {
              mac->TraceDisconnectWithoutContext ("TxErrHeader",
                                                  m_neighbors.GetTxErrorCallback ());
            }
        }

      // Close socket
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (interface, 0));
      NS_ASSERT (socket);
      socket->Close ();
      m_socketAddresses.erase (socket);
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No spy interfaces");
          m_neighbors.Clear ();
          m_locationService->Clear ();
          return;
        }
    }


    Ptr<Socket>
    RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
    {
      NS_LOG_FUNCTION (this << addr);
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
            m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ptr<Socket> socket = j->first;
          Ipv4InterfaceAddress iface = j->second;
          if (iface == addr)
            {
              return socket;
            }
        }
      Ptr<Socket> socket;
      return socket;
    }



    void RoutingProtocol::NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address)
    {
      NS_LOG_FUNCTION (this << " interface " << interface << " address " << address);
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (!l3->IsUp (interface))
        {
          return;
        }
      if (l3->GetNAddresses ((interface) == 1))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (interface, 0);
          Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
          if (!socket)
            {
              if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
                {
                  return;
                }
              // Create a socket to listen only on this interface
              Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                        UdpSocketFactory::GetTypeId ());
              NS_ASSERT (socket != 0);
              socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvSPY,this));
              socket->BindToNetDevice (l3->GetNetDevice (interface));
              // Bind to any IP address so that broadcasts can be received
              socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), SPY_PORT));
              socket->SetAllowBroadcast (true);
              m_socketAddresses.insert (std::make_pair (socket, iface));

              Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
            }
        }
      else
        {
          NS_LOG_LOGIC ("SPY does not work with more then one address per each interface. Ignore added address");
        }
    }

    void
    RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address)
    {
      NS_LOG_FUNCTION (this);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
      if (socket)
        {

          m_socketAddresses.erase (socket);
          Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
          if (l3->GetNAddresses (i))
            {
              Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
              // Create a socket to listen only on this interface
              Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                        UdpSocketFactory::GetTypeId ());
              NS_ASSERT (socket != 0);
              socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvSPY, this));
              // Bind to any IP address so that broadcasts can be received
              socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), SPY_PORT));
              socket->SetAllowBroadcast (true);
              m_socketAddresses.insert (std::make_pair (socket, iface));

              // Add local broadcast record to the routing table
              Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));

            }
          if (m_socketAddresses.empty ())
            {
              NS_LOG_LOGIC ("No spy interfaces");
              m_neighbors.Clear ();
              m_locationService->Clear ();
              return;
            }
        }
      else
        {
          NS_LOG_LOGIC ("Remove address not participating in SPY operation");
        }
    }

    void
    RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
    {
      NS_ASSERT (ipv4 != 0);
      NS_ASSERT (m_ipv4 == 0);

      m_ipv4 = ipv4;

      HelloIntervalTimer.SetFunction (&RoutingProtocol::HelloTimerExpire, this);
      double min = 0.0;
      double max = SPY_MAXJITTER;
      Ptr<UniformRandomVariable> prueba = CreateObject<UniformRandomVariable>();
      prueba->SetAttribute ("Min", DoubleValue (min));
      prueba->SetAttribute ("Max", DoubleValue (max));

      HelloIntervalTimer.Schedule (Seconds (prueba->GetValue (min,max)));

      //Schedule only when it has packets on queue
      CheckQueueTimer.SetFunction (&RoutingProtocol::CheckQueue, this);

      // Parity paths
      CheckParityPathsTimer.SetFunction (&RoutingProtocol::CheckParityPaths, this);

      Simulator::ScheduleNow (&RoutingProtocol::Start, this);
    }

    void
    RoutingProtocol::HelloTimerExpire ()
    {
      SendHello ();
      HelloIntervalTimer.Cancel ();
        double min = -1*SPY_MAXJITTER;
        double max = SPY_MAXJITTER;
        Ptr<UniformRandomVariable> p_Jitter = CreateObject<UniformRandomVariable>();
        p_Jitter->SetAttribute ("Min", DoubleValue (min));
        p_Jitter->SetAttribute ("Max", DoubleValue (max));

      HelloIntervalTimer.Schedule(HelloInterval+Seconds(p_Jitter->GetValue(min,max)));
    }

    void
    RoutingProtocol::SendHello ()
    {
      NS_LOG_FUNCTION (this);
      double positionX;
      double positionY;

      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();

      positionX = MM->GetPosition ().x;
      positionY = MM->GetPosition ().y;

      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ptr<Socket> socket = j->first;
          Ipv4InterfaceAddress iface = j->second;
          HelloHeader helloHeader (((uint64_t) positionX),((uint64_t) positionY));

          Ptr<Packet> packet = Create<Packet> ();
          packet->AddHeader (helloHeader);
          TypeHeader tHeader (SPY_TYPE_HELLO);
          packet->AddHeader (tHeader);
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv4Address destination;
          if (iface.GetMask () == Ipv4Mask::GetOnes ())
            {
              destination = Ipv4Address ("255.255.255.255");
            }
          else
            {
              destination = iface.GetBroadcast ();
            }
          socket->SendTo (packet, 0, InetSocketAddress (destination, SPY_PORT));

        }
    }

    bool
    RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
    {
      NS_LOG_FUNCTION (this << src);
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
            m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4InterfaceAddress iface = j->second;
          if (src == iface.GetLocal ())
            {
              return true;
            }
        }
      return false;
    }




    void
    RoutingProtocol::Start ()
    {
      NS_LOG_FUNCTION (this);
      m_queuedAddresses.clear ();

      //FIXME ajustar timer, meter valor parametrizavel
      Time tableTime ("2s");

      switch (LocationServiceName)
        {
        case SPY_LS_GOD:
          NS_LOG_DEBUG ("GodLS in use");
          m_locationService = CreateObject<GodLocationService> ();
          break;
        case SPY_LS_RLS:
          NS_LOG_UNCOND ("RLS not yet implemented");
          break;
        }

    }

    Ptr<Ipv4Route>
    RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif)
    {
      NS_LOG_FUNCTION (this << hdr);
      m_lo = m_ipv4->GetNetDevice (0);
      NS_ASSERT (m_lo != 0);
      Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
      rt->SetDestination (hdr.GetDestination ());

      std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
      if (oif)
        {
          // Iterate to find an address on the oif device
          for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
            {
              Ipv4Address addr = j->second.GetLocal ();
              int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
              if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
                {
                  rt->SetSource (addr);
                  break;
                }
            }
        }
      else
        {
          rt->SetSource (j->second.GetLocal ());
        }
      NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid SPY source address not found");
      rt->SetGateway (Ipv4Address ("127.0.0.1"));
      rt->SetOutputDevice (m_lo);
      return rt;
    }


    int
    RoutingProtocol::GetProtocolNumber (void) const
    {
      return SPY_PORT;
    }

    void
    RoutingProtocol::AddHeaders (Ptr<Packet> p, Ipv4Address source, Ipv4Address destination, uint8_t protocol, Ptr<Ipv4Route> route)
    {

      NS_LOG_FUNCTION (this << " source " << source << " destination " << destination);
    
      Vector myPos;
      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
      myPos.x = MM->GetPosition ().x;
      myPos.y = MM->GetPosition ().y;  
    
      Ipv4Address nextHop;

      DisjointHeader disHdr;
      p->RemoveHeader(disHdr);
    
      PacketKey key = std::make_tuple(source, destination, disHdr.GetPathId());

      p->AddHeader(disHdr);

      if(m_neighbors.isNeighbour (key, destination))
      {
          nextHop = destination;
      }

      else
      {
          nextHop = m_neighbors.BestNeighbor (key, m_locationService->GetPosition (destination), myPos);
      }

      uint16_t positionX = 0;
      uint16_t positionY = 0;
      uint32_t hdrTime = 0;

      if(destination != m_ipv4->GetAddress (1, 0).GetBroadcast ())
      {
          positionX = m_locationService->GetPosition (destination).x;
          positionY = m_locationService->GetPosition (destination).y;
          hdrTime = (uint32_t) m_locationService->GetEntryUpdateTime (destination).GetSeconds ();
      }

      PositionHeader posHeader (positionX, positionY,  hdrTime, (uint64_t) 0,(uint64_t) 0, (uint8_t) 0, myPos.x, myPos.y); 
      p->AddHeader (posHeader);
      TypeHeader tHeader (SPY_TYPE_POS);
      p->AddHeader (tHeader);

      m_downTarget (p, source, destination, protocol, route);
    }

    bool
    RoutingProtocol::Forwarding (Ptr<const Packet> packet, const Ipv4Header & header,
                                UnicastForwardCallback ucb, ErrorCallback ecb)
    {
      Ptr<Packet> p = packet->Copy ();
      NS_LOG_FUNCTION (this);
      Ipv4Address dst = header.GetDestination ();
      Ipv4Address origin = header.GetSource ();
      
      m_neighbors.Purge ();
      
      uint32_t updated = 0;
      Vector Position;
      Vector RecPosition;
      uint8_t inRec = 0;

      TypeHeader tHeader (SPY_TYPE_POS);
      PositionHeader hdr;
      p->RemoveHeader (tHeader);
      if (!tHeader.IsValid ())
      {
          NS_LOG_DEBUG ("SPY message " << p->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
          return false;     // drop
      }

      DisjointHeader disHdr;

      if (tHeader.Get () == SPY_TYPE_POS)
      {
          p->RemoveHeader (hdr);
          Position.x = hdr.GetDstPosx ();
          Position.y = hdr.GetDstPosy ();
          updated = hdr.GetUpdated ();
          RecPosition.x = hdr.GetRecPosx ();
          RecPosition.y = hdr.GetRecPosy ();
          inRec = hdr.GetInRec ();

          p->RemoveHeader(disHdr);
      }

      PacketKey key = std::make_tuple(origin, dst, disHdr.GetPathId());

      if (disHdr.GetLastHop() == disHdr.GetLastForwarder() && m_neighbors.HasNotForward(key))
      {
          disHdr.SetLastForwarder(m_ipv4->GetAddress(1, 0).GetLocal());
          disHdr.SetParity(disHdr.GetParity() == 0 ? 1 : 0);

          p->AddHeader(disHdr);
          p->AddHeader (hdr);
          p->AddHeader (tHeader);

          Ipv4Address nextHop = disHdr.GetLastHop();
          Ptr<Ipv4Route> route = Create<Ipv4Route> ();

          route->SetDestination (dst);
          route->SetSource (header.GetSource ());
          route->SetGateway (nextHop);
          route->SetOutputDevice (m_ipv4->GetNetDevice (1));
          route->SetDestination (header.GetDestination ());

          NS_ASSERT (route != 0);
          NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetOutputDevice ());                
          NS_LOG_LOGIC (route->GetOutputDevice () << " forwarding to " << dst << " from " << origin << " through " << route->GetGateway () << " packet " << p->GetUid ());
          
          ucb (route, p, header);
          return true;
      }

      Ipv4Address lastHop = disHdr.GetLastHop();

      if (disHdr.GetLastHop() != disHdr.GetLastForwarder())
      {
          m_neighbors.AddNotSend(key, disHdr.GetLastForwarder());
      }

      else if (m_ipv4->GetAddress(1, 0).GetLocal() != header.GetSource())
      {
        PacketKey inverse = std::make_tuple(origin, dst, disHdr.GetPathId() == 0 ? 1 : 0);
        m_neighbors.AddNotForward(inverse, lastHop);
      }

      disHdr.SetLastHop(m_ipv4->GetAddress(1, 0).GetLocal());
      disHdr.SetLastForwarder(m_ipv4->GetAddress(1, 0).GetLocal());
      disHdr.SetParity(disHdr.GetParity() == 0 ? 1 : 0);      

      Vector myPos;
      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
      myPos.x = MM->GetPosition ().x;
      myPos.y = MM->GetPosition ().y;  

      if(inRec == 1 && CalculateDistance (myPos, Position) < CalculateDistance (RecPosition, Position)){
        inRec = 0;
        hdr.SetInRec(0);
        NS_LOG_LOGIC ("No longer in Recovery to " << dst << " in " << myPos);
      }

      if(inRec){
        p->AddHeader(disHdr);
        p->AddHeader (hdr);
        p->AddHeader (tHeader); //put headers back so that the RecoveryMode is compatible with Forwarding and SendFromQueue
        RecoveryMode (dst, p, ucb, header);
        return true;
      }

      uint32_t myUpdated = (uint32_t) m_locationService->GetEntryUpdateTime (dst).GetSeconds ();
      if (myUpdated > updated) //check if node has an update to the position of destination
      {
          Position.x = m_locationService->GetPosition (dst).x;
          Position.y = m_locationService->GetPosition (dst).y;
          updated = myUpdated;
      }

      Ipv4Address nextHop;

      nextHop = m_neighbors.BestNeighbor (key, Position, myPos);
      if (nextHop != Ipv4Address::GetZero ())
      {
        PositionHeader posHeader (Position.x, Position.y,  updated, (uint64_t) 0, (uint64_t) 0, (uint8_t) 0, myPos.x, myPos.y);
        p->AddHeader(disHdr);
        p->AddHeader (posHeader);
        p->AddHeader (tHeader);
        
        
        Ptr<NetDevice> oif = m_ipv4->GetObject<NetDevice> ();
        Ptr<Ipv4Route> route = Create<Ipv4Route> ();
        route->SetDestination (dst);
        route->SetSource (header.GetSource ());
        route->SetGateway (nextHop);
        
        // FIXME: Does not work for multiple interfaces
        route->SetOutputDevice (m_ipv4->GetNetDevice (1));
        route->SetDestination (header.GetDestination ());

        NS_ASSERT (route != 0);
        NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetOutputDevice ());
        NS_LOG_LOGIC (route->GetOutputDevice () << " forwarding to " << dst << " from " << origin << " through " << route->GetGateway () << " packet " << p->GetUid ());
        
        ucb (route, p, header);
        return true;
      }

      hdr.SetInRec(1);
      hdr.SetRecPosx (myPos.x);
      hdr.SetRecPosy (myPos.y); 
      hdr.SetLastPosx (Position.x); //when entering Recovery, the first edge is the Dst
      hdr.SetLastPosy (Position.y); 
      
      p->AddHeader(disHdr);
      p->AddHeader (hdr);
      p->AddHeader (tHeader);
      NS_LOG_LOGIC ("Entering recovery-mode to " << dst << " in " << m_ipv4->GetAddress (1, 0).GetLocal ());
      RecoveryMode (dst, p, ucb, header);
      return true;
    }



    void
    RoutingProtocol::SetDownTarget (IpL4Protocol::DownTargetCallback callback)
    {
      m_downTarget = callback;
    }


    IpL4Protocol::DownTargetCallback
    RoutingProtocol::GetDownTarget (void) const
    {
      return m_downTarget;
    }

    Ptr<Ipv4Route>
    RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                                  Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
    {
      NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));

      if (!p)
        {
          return LoopbackRoute (header, oif);     // later
        }
      if (m_socketAddresses.empty ())
        {
          sockerr = Socket::ERROR_NOROUTETOHOST;
          NS_LOG_LOGIC ("No spy interfaces");
          Ptr<Ipv4Route> route;
          return route;
        }
      sockerr = Socket::ERROR_NOTERROR;
      Ptr<Ipv4Route> route = Create<Ipv4Route> ();
      Ipv4Address dst = header.GetDestination ();

      Vector dstPos = Vector (1, 0, 0);

      if (!(dst == m_ipv4->GetAddress (1, 0).GetBroadcast ()))
        {
          dstPos = m_locationService->GetPosition (dst);
        }

      if (CalculateDistance (dstPos, m_locationService->GetInvalidPosition ()) == 0 && m_locationService->IsInSearch (dst))
        {
          DeferredRouteOutputTag tag;
          if (!p->PeekPacketTag (tag))
            {
              p->AddPacketTag (tag);
            }
          return LoopbackRoute (header, oif);
        }

      Vector myPos;
      Ptr<MobilityModel> MM = m_ipv4->GetObject<MobilityModel> ();
      myPos.x = MM->GetPosition ().x;
      myPos.y = MM->GetPosition ().y;  

      Ipv4Address nextHop;

      uint8_t mypathid = GetAndChangePathId();

      PacketKey key = std::make_tuple(header.GetSource(), header.GetDestination(), mypathid);

      if(m_neighbors.isNeighbour(key, dst))
      {
          nextHop = dst;
      }
      else
      {
          nextHop = m_neighbors.BestNeighbor (key, dstPos, myPos);
      }

      DisjointHeader disHeader(path_id, 1, m_ipv4->GetAddress(1, 0).GetLocal(), m_ipv4->GetAddress(1, 0).GetLocal());

      p->AddHeader(disHeader);

      if (nextHop != Ipv4Address::GetZero ())
        {
          NS_LOG_DEBUG ("Destination: " << dst);

          route->SetDestination (dst);
          if (header.GetSource () == Ipv4Address ("102.102.102.102"))
            {
              route->SetSource (m_ipv4->GetAddress (1, 0).GetLocal ());
            }
          else
            {
              route->SetSource (header.GetSource ());
            }
          route->SetGateway (nextHop);
          route->SetOutputDevice (m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (route->GetSource ())));
          route->SetDestination (header.GetDestination ());
          NS_ASSERT (route != 0);
          NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetSource ());
          if (oif != 0 && route->GetOutputDevice () != oif)
            {
              NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
              sockerr = Socket::ERROR_NOROUTETOHOST;
              return Ptr<Ipv4Route> ();
            }
          return route;
        }
      else
        {
          DeferredRouteOutputTag tag;
          if (!p->PeekPacketTag (tag))
            {
              p->AddPacketTag (tag); 
            }
          return LoopbackRoute (header, oif);     //in RouteInput the recovery-mode is called
        }

    }

    int RoutingProtocol::GetAndChangePathId() {
        int atual = path_id;
        path_id = 1 - path_id;
        return atual;
    }


    void RoutingProtocol::AddParityPath(Ipv4Address source, uint8_t pathid, uint8_t parity)
    {
        std::map<Ipv4Address, std::tuple<uint8_t, uint8_t, Time>>::iterator it = parity_paths.find(source);

        if (it != parity_paths.end())
        {
            if (path_id == 0)
            {
              std::get<0>(it->second) = parity + 1;
            }

            if (path_id == 1)
            {
              std::get<1>(it->second) = parity + 1;
            }

            std::get<2>(it->second) = Simulator::Now ();
        }

        else
        {
            uint8_t first = 0;
            uint8_t second = 0;

            if (path_id == 0)
            {
                first = parity + 1;
            }

            if (path_id == 1)
            {
                second = parity + 1;
            }

            parity_paths[source] = std::make_tuple(first, second, Simulator::Now ());
        }
    }


    void
    RoutingProtocol::CheckParityPaths ()
    {
      Time expirationTime = Seconds (4.0);
      std::vector<Ipv4Address> toRemove;

      Time now = Simulator::Now ();

      for (auto it = parity_paths.begin (); it != parity_paths.end (); ++it)
      {
        Time lastUpdate = std::get<2>(it->second);

        if (lastUpdate + expirationTime <= now)
        {
          toRemove.push_back(it->first);
        }
        
        else if (std::get<0>(it->second) != 0 && std::get<1>(it->second) != 0
         && std::get<0>(it->second) != std::get<1>(it->second))
        {
            Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable> ();
            uint8_t randomBit = uv->GetValue(0, 2);
          
            PacketKey key = std::make_tuple(it->first, m_ipv4->GetAddress(1, 0).GetLocal(), randomBit);
            Ipv4Address lastHop = m_neighbors.getLastSender(key);

            // If the address is invalid, try with the other bit
            if (lastHop == Ipv4Address::GetZero())
            {
                uint8_t otherBit = 1 - randomBit;
                key = std::make_tuple(it->first, m_ipv4->GetAddress(1, 0).GetLocal(), otherBit);
                lastHop = m_neighbors.getLastSender(key);

                if (lastHop == Ipv4Address::GetZero())
                  continue;

                randomBit = otherBit;
            }

            TakeShortcut tsHdr(m_ipv4->GetAddress(1, 0).GetLocal());
            NeighIntersection neighHdr(lastHop, m_ipv4->GetAddress(1, 0).GetLocal());

            PathId piHdr(it->first, m_ipv4->GetAddress(1, 0).GetLocal(), randomBit);

            TypeHeader tHdr(SPY_TYPE_NEIGH_INTERSECTION);

            std::map<Ipv4Address, std::pair<Vector, Time>> table = m_neighbors.GetNeighborTable();

            for (const auto& entry : table)
            {
              neighHdr.AddNeighbor(entry.first);
            }

            for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
            {
              Ptr<Socket> socket = j->first;

              Ptr<Packet> packet = Create<Packet>();

              packet->AddHeader(tsHdr);
              packet->AddHeader(neighHdr);
              packet->AddHeader(piHdr);
              packet->AddHeader(tHdr);
              
              Ipv4Address destination = lastHop;
              
              socket->SendTo (packet, 0, InetSocketAddress (destination, SPY_PORT));

            }
        }
      }

      for (const auto& ip : toRemove)
      {
        parity_paths.erase(ip);
      }

      if (!parity_paths.empty())
      {
        CheckParityPathsTimer.Schedule (Seconds (2));
      }
    }

  }
}