/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "spy-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("SpyPacket");

namespace ns3 {
  namespace spy {

    NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

    TypeHeader::TypeHeader (MessageType t = SPY_TYPE_HELLO) : m_type (t), m_valid (true)
    {
    }

    TypeId TypeHeader::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::TypeHeader")
        .SetParent<Header> ()
        .AddConstructor<TypeHeader> ()
      ;
      return tid;
    }

    TypeId TypeHeader::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t TypeHeader::GetSerializedSize () const
    {
      return 1;
    }

    void TypeHeader::Serialize (Buffer::Iterator i) const
    {
      i.WriteU8 ((uint8_t) m_type);
    }

    uint32_t TypeHeader::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;
      uint8_t type = i.ReadU8 ();
      m_valid = true;
      switch (type)
      {
        case SPY_TYPE_HELLO:
        case SPY_TYPE_POS:
          {
            m_type = (MessageType) type;
            break;
          }
        default:
          m_valid = false;
      }

      uint32_t dist = i.GetDistanceFrom (start);
      NS_ASSERT (dist == GetSerializedSize ());
      return dist;
    }

    void TypeHeader::Print (std::ostream &os) const
    {
      switch (m_type)
      {
        case SPY_TYPE_HELLO:
        {
            os << "HELLO";
            break;
        }
        case SPY_TYPE_POS:
        {
            os << "POSITION";
            break;
        }
        default:
          os << "UNKNOWN_TYPE";
      }
    }

    bool TypeHeader::operator== (TypeHeader const & o) const
    {
      return (m_type == o.m_type && m_valid == o.m_valid);
    }

    std::ostream &
    operator<< (std::ostream & os, TypeHeader const & h)
    {
      h.Print (os);
      return os;
    }

    //-----------------------------------------------------------------------------
    // HELLO
    //-----------------------------------------------------------------------------
    HelloHeader::HelloHeader (uint64_t originPosx, uint64_t originPosy)
      : m_originPosx (originPosx),
        m_originPosy (originPosy)
    {
    }

    NS_OBJECT_ENSURE_REGISTERED (HelloHeader);

    TypeId HelloHeader::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::HelloHeader")
        .SetParent<Header> ()
        .AddConstructor<HelloHeader> ()
      ;
      return tid;
    }

    TypeId HelloHeader::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t HelloHeader::GetSerializedSize () const
    {
      return 16;
    }

    void HelloHeader::Serialize (Buffer::Iterator i) const
    {
      NS_LOG_DEBUG ("Serialize X " << m_originPosx << " Y " << m_originPosy);


      i.WriteHtonU64 (m_originPosx);
      i.WriteHtonU64 (m_originPosy);

    }

    uint32_t HelloHeader::Deserialize (Buffer::Iterator start)
    {

      Buffer::Iterator i = start;

      m_originPosx = i.ReadNtohU64 ();
      m_originPosy = i.ReadNtohU64 ();

      NS_LOG_DEBUG ("Deserialize X " << m_originPosx << " Y " << m_originPosy);

      uint32_t dist = i.GetDistanceFrom (start);
      NS_ASSERT (dist == GetSerializedSize ());
      return dist;
    }

    void HelloHeader::Print (std::ostream &os) const
    {
      os << " PositionX: " << m_originPosx
        << " PositionY: " << m_originPosy;
    }

    std::ostream &
    operator<< (std::ostream & os, HelloHeader const & h)
    {
      h.Print (os);
      return os;
    }

    bool HelloHeader::operator== (HelloHeader const & o) const
    {
      return (m_originPosx == o.m_originPosx && m_originPosy == o.m_originPosy);
    }

    //-----------------------------------------------------------------------------
    // Position
    //-----------------------------------------------------------------------------
    PositionHeader::PositionHeader (uint64_t dstPosx, uint64_t dstPosy, uint32_t updated, uint64_t recPosx, uint64_t recPosy, uint8_t inRec, uint64_t lastPosx, uint64_t lastPosy)
      : m_dstPosx (dstPosx),
        m_dstPosy (dstPosy),
        m_updated (updated),
        m_recPosx (recPosx),
        m_recPosy (recPosy),
        m_inRec (inRec),
        m_lastPosx (lastPosx),
        m_lastPosy (lastPosy)
    {
    }

    NS_OBJECT_ENSURE_REGISTERED (PositionHeader);

    TypeId PositionHeader::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::PositionHeader")
        .SetParent<Header> ()
        .AddConstructor<PositionHeader> ()
      ;
      return tid;
    }

    TypeId PositionHeader::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t PositionHeader::GetSerializedSize () const
    {
      return 53;
    }

    void PositionHeader::Serialize (Buffer::Iterator i) const
    {
      i.WriteU64 (m_dstPosx);
      i.WriteU64 (m_dstPosy);
      i.WriteU32 (m_updated);
      i.WriteU64 (m_recPosx);
      i.WriteU64 (m_recPosy);
      i.WriteU8 (m_inRec);
      i.WriteU64 (m_lastPosx);
      i.WriteU64 (m_lastPosy);
    }

    uint32_t PositionHeader::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;
      m_dstPosx = i.ReadU64 ();
      m_dstPosy = i.ReadU64 ();
      m_updated = i.ReadU32 ();
      m_recPosx = i.ReadU64 ();
      m_recPosy = i.ReadU64 ();
      m_inRec = i.ReadU8 ();
      m_lastPosx = i.ReadU64 ();
      m_lastPosy = i.ReadU64 ();

      uint32_t dist = i.GetDistanceFrom (start);
      NS_ASSERT (dist == GetSerializedSize ());
      return dist;
    }

    void PositionHeader::Print (std::ostream &os) const
    {
      os << " PositionX: "  << m_dstPosx
        << " PositionY: " << m_dstPosy
        << " Updated: " << m_updated
        << " RecPositionX: " << m_recPosx
        << " RecPositionY: " << m_recPosy
        << " inRec: " << m_inRec
        << " LastPositionX: " << m_lastPosx
        << " LastPositionY: " << m_lastPosy;
    }

    std::ostream &
    operator<< (std::ostream & os, PositionHeader const & h)
    {
      h.Print (os);
      return os;
    }

    bool
    PositionHeader::operator== (PositionHeader const & o) const
    {
      return (m_dstPosx == o.m_dstPosx && m_dstPosy == o.m_dstPosy && m_updated == o.m_updated && m_recPosx == o.m_recPosx && m_recPosy == o.m_recPosy && m_inRec == o.m_inRec && m_lastPosx == o.m_lastPosx && m_lastPosy == o.m_lastPosy);
    }

    //-----------------------------------------------------------------------------
    // DISJOINT
    //-----------------------------------------------------------------------------

    DisjointHeader::DisjointHeader(uint8_t p_id = 0, uint8_t pa = 0, Ipv4Address lh = 0, Ipv4Address lf = 0) : path_id(p_id), parity(pa), last_hop(lh), last_forwarder(lf) 
    {
    }

    TypeId DisjointHeader::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::DisjointHeader")
        .SetParent<Header> ()
        .AddConstructor<DisjointHeader> ()
      ;
      return tid;
    }

    TypeId DisjointHeader::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t DisjointHeader::GetSerializedSize () const
    {
      return 10;
    }

    void DisjointHeader::Serialize (Buffer::Iterator i) const
    {
      i.WriteU8 ((uint8_t) path_id);
      i.WriteU8 ((uint8_t) parity);
      i.WriteU32 ((uint32_t) last_hop.Get());
      i.WriteU32 ((uint32_t) last_forwarder.Get());
    }

    uint32_t DisjointHeader::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;

      path_id = i.ReadU8();
      parity = i.ReadU8();
      last_hop = Ipv4Address(i.ReadU32());
      last_forwarder = Ipv4Address(i.ReadU32());

      uint32_t dist = i.GetDistanceFrom(start);
      NS_ASSERT(dist == GetSerializedSize());
      return dist;
    }

    void DisjointHeader::Print (std::ostream &os) const
    {
      os << " PathId: "  << path_id
        << " Parity: " << parity
        << " Last Hop: " << last_hop
        << " Last Forwarder: " << last_forwarder;
    }

    bool DisjointHeader::operator== (DisjointHeader const & o) const
    {
      return (path_id == o.path_id && parity == o.parity && last_hop == o.last_hop && last_forwarder == o.last_forwarder);
    }

    std::ostream &
    operator<< (std::ostream & os, DisjointHeader const & h)
    {
      h.Print (os);
      return os;
    }

  }
}





