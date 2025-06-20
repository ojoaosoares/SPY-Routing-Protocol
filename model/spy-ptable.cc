#include "spy-ptable.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("SpyTable");


namespace ns3 {
  namespace spy {

    /*
      SPY position table
    */

    PositionTable::PositionTable ()
    {
      m_txErrorCallback = MakeCallback (&PositionTable::ProcessTxError, this);
      m_entryLifeTime = Seconds (2); //FIXME fazer isto parametrizavel de acordo com tempo de hello
    }

    Time PositionTable::GetEntryUpdateTime (Ipv4Address id)
    {
      if (id == Ipv4Address::GetZero ())
      {
          return Time (Seconds (0));
      }

      std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find (id);
      return i->second.second;
    }

    /**
     * \brief Adds entry in position table
     */
    void PositionTable::AddEntry (Ipv4Address id, Vector position)
    {
      std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find (id);
      if (i != m_table.end () || id.IsEqual (i->first))
      {
          m_table.erase (id);
      }
      
      m_table.insert (std::make_pair (id, std::make_pair (position, Simulator::Now ())));
    }

    /**
     * \brief Deletes entry in position table
     */
    void PositionTable::DeleteEntry (Ipv4Address id)
    {
      m_table.erase (id);
    }

    /**
     * \brief Gets position from position table
     * \param id Ipv4Address to get position from
     * \return Position of that id or NULL if not known
     */
    Vector PositionTable::GetPosition (Ipv4Address id)
    {

      NodeList::Iterator listEnd = NodeList::End ();
      for (NodeList::Iterator i = NodeList::Begin (); i != listEnd; i++)
        {
          Ptr<Node> node = *i;
          if (node->GetObject<Ipv4> ()->GetAddress (1, 0).GetLocal () == id)
            {
              return node->GetObject<MobilityModel> ()->GetPosition ();
            }
        }
      return PositionTable::GetInvalidPosition ();

    }

    /**
     * \brief Checks if a node is a neighbour
     * \param id Ipv4Address of the node to check
     * \return True if the node is neighbour, false otherwise
     */
    bool PositionTable::isNeighbour (const PacketKey& pktKey, Ipv4Address id)
    {
        std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.find (id);

        if ((i != m_table.end () || id.IsEqual (i->first)) && !HasNotSend(pktKey, id))
        {
          return true;
        }

        return false;
    }


    /**
     * \brief remove entries with expired lifetime
     */
    void PositionTable::Purge ()
    {

        if(!m_table.empty ())
        {
            
          std::list<Ipv4Address> toErase;

          std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i = m_table.begin ();
          std::map<Ipv4Address, std::pair<Vector, Time> >::iterator listEnd = m_table.end ();
          
          for (; !(i == listEnd); i++)
            {

              if (m_entryLifeTime + GetEntryUpdateTime (i->first) <= Simulator::Now ())
                {
                  toErase.insert (toErase.begin (), i->first);

                }
            }
          toErase.unique ();

          std::list<Ipv4Address>::iterator end = toErase.end ();

          for (std::list<Ipv4Address>::iterator it = toErase.begin (); it != end; ++it)
            {

              m_table.erase (*it);

            }
        }

        // Purge notSend
        if (!notSend.empty())
        {
            std::list<std::pair<PacketKey, Ipv4Address> > toErase;

            for (std::map<std::pair<PacketKey, Ipv4Address>, Time>::iterator it = notSend.begin ();
                it != notSend.end (); ++it)
            {
                if (m_entryLifeTime + it->second <= Simulator::Now ())
                {
                    toErase.push_back(it->first);
                }
            }

            for (std::list<std::pair<PacketKey, Ipv4Address> >::iterator it = toErase.begin ();
                it != toErase.end (); ++it)
            {
                notSend.erase(*it);
            }
        }

        // Purge notForward
        if (!notForward.empty())
        {
            std::list<PacketKey> toErase;

            for (std::map<PacketKey, Time>::iterator it = notForward.begin ();
                it != notForward.end (); ++it)
            {
                if (m_entryLifeTime + it->second <= Simulator::Now ())
                {
                    toErase.push_back(it->first);
                }
            }

            for (std::list<PacketKey>::iterator it = toErase.begin ();
                it != toErase.end (); ++it)
            {
                notForward.erase(*it);
            }
        }
    }

    /**
     * \brief clears all entries
     */
    void PositionTable::Clear ()
    {
      m_table.clear ();
    }

    /**
     * \brief Gets next hop according to SPY protocol
     * \param position the position of the destination node
     * \param nodePos the position of the node that has the packet
     * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
     */
    Ipv4Address PositionTable::BestNeighbor (const PacketKey& pktKey, Vector position, Vector nodePos)
    {
      Purge ();

      PacketKey inverseKey = std::make_tuple(std::get<0>(pktKey), std::get<1>(pktKey), std::get<2>(pktKey) != 1);

      double initialDistance = CalculateDistance (nodePos, position);

      if (m_table.empty ())
      {
        NS_LOG_DEBUG ("BestNeighbor table is empty; Position: " << position);
        return Ipv4Address::GetZero ();
      }     //if table is empty (no neighbours)

      Ipv4Address bestFoundID = m_table.begin ()->first;
      double bestFoundDistance = CalculateDistance (m_table.begin ()->second.first, position);
      std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i;
      for (i = m_table.begin (); !(i == m_table.end ()); i++)
        {
          if (HasNotSend(inverseKey, i->first))
          {
            return i->first;
          }
          
          if (bestFoundDistance > CalculateDistance (i->second.first, position) && !HasNotSend(pktKey, i->first))
          {
            bestFoundID = i->first;
            bestFoundDistance = CalculateDistance (i->second.first, position);
          }
        }

      if(initialDistance > bestFoundDistance && !HasNotSend(pktKey, bestFoundID))
        return bestFoundID;
      else
        return Ipv4Address::GetZero (); //so it enters Recovery-mode

    }


    /**
     * \brief Gets next hop according to SPY recovery-mode protocol (right hand rule)
     * \param previousHop the position of the node that sent the packet to this node
     * \param nodePos the position of the destination node
     * \return Ipv4Address of the next hop, Ipv4Address::GetZero () if no nighbour was found in greedy mode
     */
    Ipv4Address PositionTable::BestAngle (const PacketKey& pktKey, Vector previousHop, Vector nodePos)
    {
      Purge ();

      if (m_table.empty ())
        {
          NS_LOG_DEBUG ("BestNeighbor table is empty; Position: " << nodePos);
          return Ipv4Address::GetZero ();
        }     //if table is empty (no neighbours)

      double tmpAngle;
      Ipv4Address bestFoundID = Ipv4Address::GetZero ();
      double bestFoundAngle = 360;
      std::map<Ipv4Address, std::pair<Vector, Time> >::iterator i;

      for (i = m_table.begin (); !(i == m_table.end ()); i++)
      {
          tmpAngle = GetAngle(nodePos, previousHop, i->second.first);
          if (bestFoundAngle > tmpAngle && tmpAngle != 0 && !HasNotSend(pktKey, i->first))
          {
            bestFoundID = i->first;
            bestFoundAngle = tmpAngle;
          }
      }

      if(bestFoundID == Ipv4Address::GetZero ()) //only if the only neighbour is who sent the packet
      {
          bestFoundID = m_table.begin ()->first;
      }
      
      return bestFoundID;
    }


    //Gives angle between the vector CentrePos-Refpos to the vector CentrePos-node counterclockwise
    double PositionTable::GetAngle (Vector centrePos, Vector refPos, Vector node){
      double const PI = 4*atan(1);

      std::complex<double> A = std::complex<double>(centrePos.x,centrePos.y);
      std::complex<double> B = std::complex<double>(node.x,node.y);
      std::complex<double> C = std::complex<double>(refPos.x,refPos.y);   //Change B with C if you want angles clockwise

      std::complex<double> AB; //reference edge
      std::complex<double> AC;
      std::complex<double> tmp;
      std::complex<double> tmpCplx;

      std::complex<double> Angle;

      AB = B - A;
      AB = (real(AB)/norm(AB)) + (std::complex<double>(0.0,1.0)*(imag(AB)/norm(AB)));

      AC = C - A;
      AC = (real(AC)/norm(AC)) + (std::complex<double>(0.0,1.0)*(imag(AC)/norm(AC)));

      tmp = log(AC/AB);
      tmpCplx = std::complex<double>(0.0,-1.0);
      Angle = tmp*tmpCplx;
      Angle *= (180/PI);
      if (real(Angle)<0)
        Angle = 360+real(Angle);

      return real(Angle);

    }





    /**
     * \ProcessTxError
     */
    void PositionTable::ProcessTxError (WifiMacHeader const & hdr)
    {
    }



    //FIXME ainda preciso disto agr que o LS ja n está aqui???????

    /**
     * \brief Returns true if is in search for destionation
     */
    bool PositionTable::IsInSearch (Ipv4Address id)
    {
      return false;
    }

    bool PositionTable::HasPosition (Ipv4Address id)
    {
      return true;
    }


    // prohibited functions

    void PositionTable::AddNotSend(const PacketKey& pktKey, const Ipv4Address& receiver)
    {
        std::pair<PacketKey, Ipv4Address> key = std::make_pair(pktKey, receiver);
        std::map<std::pair<PacketKey, Ipv4Address>, Time>::iterator it = notSend.find(key);

        if (it != notSend.end())
        {
            notSend.erase(it);
        }

        notSend.insert(std::make_pair(key, Simulator::Now()));
    }

    bool PositionTable::HasNotSend(const PacketKey& pktKey, const Ipv4Address& receiver)
    {
        std::pair<PacketKey, Ipv4Address> key = std::make_pair(pktKey, receiver);
        std::map<std::pair<PacketKey, Ipv4Address>, Time>::iterator it = notSend.find(key);

        return (it != notSend.end());
    }

    void PositionTable::AddNotForward(const PacketKey& pktKey)
    {
        std::map<PacketKey, Time>::iterator it = notForward.find(pktKey);

        if (it != notForward.end())
        {
            notForward.erase(it);
        }

        notForward.insert(std::make_pair(pktKey, Simulator::Now()));
    }

    bool PositionTable::HasNotForward(const PacketKey& pktKey) {
        std::map<PacketKey, Time>::iterator it = notForward.find(pktKey);

        return (it != notForward.end());  
    }
  }   // spy
} // ns3
