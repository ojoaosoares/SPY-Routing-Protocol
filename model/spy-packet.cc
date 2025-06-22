/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "spy-packet.h"
#include "ns3/address-utils.h"
#include "ns3/packet.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("SpyPacket");

namespace ns3 {
  namespace spy {

    NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

    TypeHeader::TypeHeader (MessageType t) : m_type (t), m_valid (true)
    {
    }

    TypeHeader::TypeHeader() : m_type(SPY_TYPE_HELLO), m_valid(true)
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
        case SPY_TYPE_IN_ANALYSIS:
        case SPY_TYPE_NEIGH_INTERSECTION:
        case SPY_TYPE_SET_PATH:
        case SPY_TYPE_TAKE_SHORTCUT:
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
      i.WriteHtonU64 (m_dstPosx);
      i.WriteHtonU64 (m_dstPosy);
      i.WriteHtonU32 (m_updated);
      i.WriteHtonU64 (m_recPosx);
      i.WriteHtonU64 (m_recPosy);
      i.WriteU8 (m_inRec);
      i.WriteHtonU64 (m_lastPosx);
      i.WriteHtonU64 (m_lastPosy);
    }

    uint32_t PositionHeader::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;
      m_dstPosx = i.ReadNtohU64 ();
      m_dstPosy = i.ReadNtohU64 ();
      m_updated = i.ReadNtohU32 ();
      m_recPosx = i.ReadNtohU64 ();
      m_recPosy = i.ReadNtohU64 ();
      m_inRec = i.ReadU8 ();
      m_lastPosx = i.ReadNtohU64 ();
      m_lastPosy = i.ReadNtohU64 ();

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

    DisjointHeader::DisjointHeader(uint8_t p_id, uint8_t pa, Ipv4Address lh, Ipv4Address lf) : path_id(p_id), parity(pa), last_hop(lh), last_forwarder(lf) 
    {
    }

    NS_OBJECT_ENSURE_REGISTERED (DisjointHeader);

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
      i.WriteHtonU32 ((uint32_t) last_hop.Get());
      i.WriteHtonU32 ((uint32_t) last_forwarder.Get());
    }

    uint32_t DisjointHeader::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;

      path_id = i.ReadU8();
      parity = i.ReadU8();
      last_hop = Ipv4Address(i.ReadNtohU32());
      last_forwarder = Ipv4Address(i.ReadNtohU32());

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

    //-----------------------------------------------------------------------------
    // PATHID HEADER
    //-----------------------------------------------------------------------------


    NS_OBJECT_ENSURE_REGISTERED (PathId);

    PathId::PathId (Ipv4Address s, Ipv4Address d, uint8_t id) : source(s), dest(d), m_id(id)
    {
    }

    TypeId PathId::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::PathId")
        .SetParent<Header> ()
        .AddConstructor<PathId> ()
      ;
      return tid;
    }

    TypeId PathId::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t PathId::GetSerializedSize () const
    {
      return 9;
    }

    void PathId::Serialize (Buffer::Iterator i) const
    {
      i.WriteHtonU32(source.Get());
      i.WriteHtonU32(dest.Get());
      i.WriteU8 ((uint8_t) m_id);
    }

    uint32_t PathId::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;
      source = Ipv4Address(i.ReadNtohU32());
      dest = Ipv4Address(i.ReadNtohU32());
      m_id = i.ReadU8 ();

      uint32_t dist = i.GetDistanceFrom (start);
      NS_ASSERT (dist == GetSerializedSize ());
      return dist;
    }

    void PathId::Print (std::ostream &os) const
    {
      os << "PATH ID " << m_id; 
    }

    bool PathId::operator== (PathId const & o) const
    {
      return (m_id == o.m_id);
    }

    std::ostream &
    operator<< (std::ostream & os, PathId const & h)
    {
      h.Print (os);
      return os;
    }

    //-----------------------------------------------------------------------------
    // NEIGHINTERSECTION HEADER
    //-----------------------------------------------------------------------------

    NeighIntersection::NeighIntersection(Ipv4Address s, Ipv4Address d) : source(s), dest(d)
    {
    }

    NS_OBJECT_ENSURE_REGISTERED (NeighIntersection);

    TypeId NeighIntersection::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::NeighIntersection")
        .SetParent<Header> ()
        .AddConstructor<NeighIntersection> ()
      ;
      return tid;
    }

    TypeId NeighIntersection::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t NeighIntersection::GetSerializedSize () const {
        return 1 + m_neighbors.size() * 4;
    }

    void NeighIntersection::Serialize (Buffer::Iterator start) const {
        start.WriteU8(static_cast<uint8_t>(m_neighbors.size()));
      
        for (const auto& addr : m_neighbors) {
            start.WriteHtonU32(addr.Get());
        }
    }

    uint32_t NeighIntersection::Deserialize (Buffer::Iterator start) {

        Buffer::Iterator i = start;
        uint8_t count = i.ReadU8();

        m_neighbors.clear();
        for (uint8_t j = 0; j < count; ++j) {
            uint32_t raw = i.ReadNtohU32();
            m_neighbors.push_back(Ipv4Address(raw));
        }
        uint32_t dist = i.GetDistanceFrom(start);
        NS_ASSERT(dist == GetSerializedSize());
        return dist;
    }

    void NeighIntersection::AddNeighbor(Ipv4Address addr) {
        m_neighbors.push_back(addr);
    }

    const std::vector<Ipv4Address>& NeighIntersection::GetNeighbors() const {
        return m_neighbors;
    }

    Ipv4Address NeighIntersection::PopLastNeighbor()
    {
      if (m_neighbors.empty())
      {
        return Ipv4Address::GetZero();
      }

      Ipv4Address last = m_neighbors.back();
      m_neighbors.pop_back();
      return last;
    }

    void NeighIntersection::ClearNeighbors() {
        m_neighbors.clear();
    }

    void NeighIntersection::Print (std::ostream &os) const
    {
      os << " Neighbors: ";
      for (const auto& addr : m_neighbors)
      {
        os << addr << " ";
      }
    }

    bool NeighIntersection::operator== (NeighIntersection const & o) const
    {
      return m_neighbors == o.m_neighbors;
    }

    std::ostream &
    operator<< (std::ostream & os, NeighIntersection const & h)
    {
      h.Print (os);
      return os;
    }

    //-----------------------------------------------------------------------------
    // TAKESHORTCUT HEADER
    //-----------------------------------------------------------------------------

    TakeShortcut::TakeShortcut(Ipv4Address sc) : shortcut(sc)
    {
    }

    NS_OBJECT_ENSURE_REGISTERED (TakeShortcut);

    TypeId TakeShortcut::GetTypeId ()
    {
      static TypeId tid = TypeId ("ns3::spy::TakeShortcut")
        .SetParent<Header> ()
        .AddConstructor<TakeShortcut> ()
      ;
      return tid;
    }

    TypeId TakeShortcut::GetInstanceTypeId () const
    {
      return GetTypeId ();
    }

    uint32_t TakeShortcut::GetSerializedSize () const
    {
      return 4;
    }

    void TakeShortcut::Serialize (Buffer::Iterator i) const
    {
      i.WriteHtonU32 ((uint32_t) shortcut.Get());
    }

    uint32_t TakeShortcut::Deserialize (Buffer::Iterator start)
    {
      Buffer::Iterator i = start;
      shortcut = Ipv4Address(i.ReadNtohU32());
    
      uint32_t dist = i.GetDistanceFrom(start);
      NS_ASSERT(dist == GetSerializedSize());
      return dist;
    }

    void TakeShortcut::Print (std::ostream &os) const
    {
      os << " Shortcut: "  << shortcut;
    }

    bool TakeShortcut::operator== (TakeShortcut const & o) const
    {
      return (shortcut == o.shortcut);
    }

    std::ostream &
    operator<< (std::ostream & os, TakeShortcut const & h)
    {
      h.Print (os);
      return os;
    }

  }
}