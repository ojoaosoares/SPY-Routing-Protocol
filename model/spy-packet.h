/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef SPY_PACKET_H
#define SPY_PACKET_H

#include <iostream>
#include "ns3/header.h"
#include "ns3/enum.h"
#include "ns3/ipv4-address.h"
#include <map>
#include "ns3/nstime.h"
#include "ns3/vector.h"

namespace ns3 {
  namespace spy {

    enum MessageType
    {
      SPY_TYPE_HELLO  = 1,                        //!< SPY_TYPE_HELLO
      SPY_TYPE_POS = 2,                           //!< SPY_TYPE_POS
      SPY_TYPE_NEIGH_INTERSECTION = 3,            //!< SPY_TYPE_DISJOINT
      SPY_TYPE_TAKE_SHORTCUT = 4                  //!< SPY_TYPE_TAKE_SHORTCUT
    };

    /**
     * \ingroup spy
     * \brief spy types
     */
    class TypeHeader : public Header
    {
      public:
        /// c-tor
        TypeHeader (MessageType t);
        TypeHeader ();

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        /// Return type
        MessageType Get () const
        {
          return m_type;
        }
        /// Check that type if valid
        bool IsValid () const
        {
          return m_valid; //FIXME that way it wont work
        }
        bool operator== (TypeHeader const & o) const;
      private:
        MessageType m_type;
        bool m_valid;
    };

    std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

    class HelloHeader : public Header
    {
      public:
        /// c-tor
        HelloHeader (uint64_t originPosx = 0, uint64_t originPosy = 0);

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        ///\name Fields
        //\{
        void SetOriginPosx (uint64_t posx)
        {
          m_originPosx = posx;
        }
        uint64_t GetOriginPosx () const
        {
          return m_originPosx;
        }
        void SetOriginPosy (uint64_t posy)
        {
          m_originPosy = posy;
        }
        uint64_t GetOriginPosy () const
        {
          return m_originPosy;
        }
        //\}

        bool operator== (HelloHeader const & o) const;
      private:
        uint64_t         m_originPosx;          ///< Originator Position x
        uint64_t         m_originPosy;          ///< Originator Position x
    };

    std::ostream & operator<< (std::ostream & os, HelloHeader const &);

    class PositionHeader : public Header
    {
      public:
        /// c-tor
        PositionHeader (uint64_t dstPosx = 0, uint64_t dstPosy = 0, uint32_t updated = 0, uint64_t recPosx = 0, uint64_t recPosy = 0, uint8_t inRec  = 0, uint64_t lastPosx = 0, uint64_t lastPosy = 0);

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        ///\name Fields
        //\{
        void SetDstPosx (uint64_t posx)
        {
          m_dstPosx = posx;
        }
        uint64_t GetDstPosx () const
        {
          return m_dstPosx;
        }
        void SetDstPosy (uint64_t posy)
        {
          m_dstPosy = posy;
        }
        uint64_t GetDstPosy () const
        {
          return m_dstPosy;
        }
        void SetUpdated (uint32_t updated)
        {
          m_updated = updated;
        }
        uint32_t GetUpdated () const
        {
          return m_updated;
        }
        void SetRecPosx (uint64_t posx)
        {
          m_recPosx = posx;
        }
        uint64_t GetRecPosx () const
        {
          return m_recPosx;
        }
        void SetRecPosy (uint64_t posy)
        {
          m_recPosy = posy;
        }
        uint64_t GetRecPosy () const
        {
          return m_recPosy;
        }
        void SetInRec (uint8_t rec)
        {
          m_inRec = rec;
        }
        uint8_t GetInRec () const
        {
          return m_inRec;
        }
        void SetLastPosx (uint64_t posx)
        {
          m_lastPosx = posx;
        }
        uint64_t GetLastPosx () const
        {
          return m_lastPosx;
        }
        void SetLastPosy (uint64_t posy)
        {
          m_lastPosy = posy;
        }
        uint64_t GetLastPosy () const
        {
          return m_lastPosy;
        }

        bool operator== (PositionHeader const & o) const;
      private:
        uint64_t         m_dstPosx;          ///< Destination Position x
        uint64_t         m_dstPosy;          ///< Destination Position x
        uint32_t         m_updated;          ///< Time of last update
        uint64_t         m_recPosx;          ///< x of position that entered Recovery-mode
        uint64_t         m_recPosy;          ///< y of position that entered Recovery-mode
        uint8_t          m_inRec;          ///< 1 if in Recovery-mode, 0 otherwise
        uint64_t         m_lastPosx;          ///< x of position of previous hop
        uint64_t         m_lastPosy;          ///< y of position of previous hop
    };

    std::ostream & operator<< (std::ostream & os, PositionHeader const &);
    
    class DisjointHeader : public Header
    {
      public:
        /// c-tor
        DisjointHeader(uint8_t p_id = 0, uint8_t pa = 0, Ipv4Address lh = Ipv4Address::GetZero(), Ipv4Address lf = Ipv4Address::GetZero());

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        ///\name Fields
        //\{
        void SetPathId (uint8_t pid)
        {
          path_id = pid;
        }

        uint8_t GetPathId () const
        {
          return path_id;
        }

        void SetParity (uint8_t p)
        {
          parity = p;
        }

        uint8_t GetParity () const
        {
          return parity;
        }

        void SetLastHop (Ipv4Address lh)
        {
          last_hop = lh;
        }

        Ipv4Address GetLastHop () const
        {
          return last_hop;
        }

        void SetLastForwarder (Ipv4Address lf)
        {
          last_forwarder = lf;
        }

        Ipv4Address GetLastForwarder() const
        {
          return last_forwarder;
        }

        bool operator== (DisjointHeader const & o) const;
      private:
        uint8_t path_id;
        uint8_t parity;
        Ipv4Address last_hop;
        Ipv4Address last_forwarder;
    };

    class NeighIntersection : public Header
    {
      public:
        /// c-tor
        NeighIntersection(Ipv4Address s, Ipv4Address d);

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        ///\name Fields
        //\{
        void SetSource (Ipv4Address s)
        {
          source = s;
        }

        Ipv4Address GetSource() const
        {
          return source;
        }

        void SetDest (Ipv4Address d)
        {
          dest = d;
        }

        Ipv4Address GetDest() const
        {
          return dest;
        }

        void AddNeighbor(ns3::Ipv4Address addr);
        const std::vector<ns3::Ipv4Address>& GetNeighbors() const;
        void ClearNeighbors();

        bool operator== (NeighIntersection const & o) const;
      private:
          Ipv4Address dest;
          Ipv4Address source;
          std::vector<ns3::Ipv4Address> m_neighbors;
        
    };

    std::ostream & operator<< (std::ostream & os, NeighIntersection const & h);

    class TakeShortcut : public Header
    {
      public:
        /// c-tor
        TakeShortcut(Ipv4Address sc = Ipv4Address::GetZero());

        ///\name Header serialization/deserialization
        //\{
        static TypeId GetTypeId ();
        TypeId GetInstanceTypeId () const;
        uint32_t GetSerializedSize () const;
        void Serialize (Buffer::Iterator start) const;
        uint32_t Deserialize (Buffer::Iterator start);
        void Print (std::ostream &os) const;
        //\}

        ///\name Fields
        //\{
        void SetShortcut (Ipv4Address sc)
        {
          shortcut = sc;
        }

        Ipv4Address GetShortcut () const
        {
          return shortcut;
        }

        bool operator== (TakeShortcut const & o) const;
      private:
        Ipv4Address shortcut;
    };

    std::ostream & operator<< (std::ostream & os, TakeShortcut const & h);

  }
}
#endif /* SPY_PACKET_H */
