// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_P2P_EMBARGOMAN_H
#define UNITE_P2P_EMBARGOMAN_H

#include <stdint.h>
#include <chrono>
#include <memory>
#include <set>

#include <primitives/transaction.h>
#include <util.h>

namespace p2p {

// The same as in net.h, but including net.h will create circular dependency
using NodeId = int64_t;

//! \brief Extracted side effects of Embargo Man (dandelion-lite)
//! Dandelion-lite heavily relies on:
//! Random numbers (embargo delays and relay selection)
//! Time (is embargo due?)
//! Network side effects(outbound nodes, tx sending)
//!
//! In order to be able to unit test it we extract all those side effect
//! management to this class
class EmbargoManSideEffects {
 public:
  using EmbargoTime = int64_t;

  virtual EmbargoTime GetNextEmbargoTime() = 0;
  virtual bool IsEmbargoDue(EmbargoTime time) = 0;

  virtual std::set<NodeId> GetOutboundNodes() = 0;
  virtual size_t RandRange(size_t max_excluding) = 0;
  virtual bool SendTxInv(NodeId node_id, const uint256 &tx_hash) = 0;
  virtual void SendTxInvToAll(const uint256 &tx) = 0;

  virtual ~EmbargoManSideEffects() = default;
};

//! \brief Embargo manager, implements Dandelion lite privacy enhancement protocol
class EmbargoMan {
 public:
  explicit EmbargoMan(size_t timeouts_to_switch_relay,
                      std::unique_ptr<EmbargoManSideEffects> side_effects);

  bool SendTransactionAndEmbargo(const CTransaction &tx);

  void FluffPendingEmbargoes();

  bool IsEmbargoed(const uint256 &tx_hash) const;

  bool IsEmbargoedFor(const uint256 &tx_hash, NodeId node) const;

  void OnTxInv(const uint256 &tx_hash, NodeId from);

 private:
  using EmbargoTime = EmbargoManSideEffects::EmbargoTime;
  const size_t m_timeouts_to_switch_relay;
  std::unique_ptr<EmbargoManSideEffects> m_side_effects;
  boost::optional<NodeId> m_relay;
  size_t m_timeouts_in_a_row = 0;

  // Locking policy: lock everything with m_relay_cs, except what accesses
  // m_embargo_to_tx and m_embargoes - this might create deadlocks
  // Never send something to network under m_embargo_cs lock
  mutable CCriticalSection m_relay_cs;
  mutable CCriticalSection m_embargo_cs;

  std::multimap<EmbargoTime, uint256> m_embargo_to_tx GUARDED_BY(m_embargo_cs);

  struct Embargo {
    Embargo(NodeId relay, EmbargoTime embargo_time);
    NodeId relay;
    EmbargoTime embargo_time;
  };

  std::map<uint256, Embargo> m_embargoes GUARDED_BY(m_embargo_cs);

  bool SendToAndRemember(NodeId relay, const CTransaction &tx);

  EmbargoTime GetEmbargoTime(const CTransaction &tx);

 protected:
  boost::optional<NodeId> GetNewRelay();
  std::set<NodeId> m_unwanted_relays;
};

}  // namespace p2p

#endif  //UNITE_P2P_EMBARGOMAN_H
