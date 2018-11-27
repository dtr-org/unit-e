// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_DANDELION_DANDELION_H
#define UNITE_DANDELION_DANDELION_H

#include <stdint.h>
#include <chrono>
#include <memory>
#include <unordered_set>

#include <primitives/transaction.h>
#include <util.h>

namespace dandelion {

// The same as in net.h, but including net.h will create circular dependency
using NodeId = int64_t;

//! \brief Extracted side effects of dandelion-lite
//! Dandelion heavily relies on:
//! Random numbers (embargo delays and relay selection)
//! Time (is embargo due?)
//! Network side effects(outbound nodes, tx sending)
//!
//! In order to be able to unit test dandelion we extract all those side effect
//! management to this class
class SideEffects {
 public:
  using EmbargoTime = int64_t;

  virtual EmbargoTime GetNextEmbargoTime() = 0;
  virtual bool IsEmbargoDue(EmbargoTime time) = 0;

  virtual std::unordered_set<NodeId> GetOutboundNodes() = 0;
  virtual size_t RandRange(size_t maxExcluding) = 0;
  virtual bool SendTxInv(NodeId nodeId, const uint256 &txHash) = 0;
  virtual void SendTxInvToAll(const uint256 &tx) = 0;

  virtual ~SideEffects() = default;
};

//! \brief "Dandelion lite" - privacy enhancement protocol implementation
class DandelionLite {
 public:
  explicit DandelionLite(size_t timeoutsToSwitchRelay,
                         std::unique_ptr<SideEffects> sideEffects);

  bool SendTransaction(const uint256 &txHash);

  void FluffPendingEmbargoes();

  bool IsEmbargoed(const uint256 &txHash) const;

  bool IsEmbargoedFor(const uint256 &txHash, NodeId node) const;

  void OnTxInv(const uint256 &txHash, NodeId from);

 private:
  const size_t m_timeoutsToSwitchRelay;
  std::unique_ptr<SideEffects> m_sideEffects;
  boost::optional<NodeId> m_relay;
  size_t m_timeoutsInARow = 0;

  // Locking policy: lock everything with m_relayCs, except what accesses
  // m_embargoCs and m_txToRelay - this might create deadlocks
  // Never send something to network under m_embargoCs lock
  mutable CCriticalSection m_relayCs;
  mutable CCriticalSection m_embargoCs;

  std::multimap<SideEffects::EmbargoTime, uint256> m_embargoToTx GUARDED_BY(m_embargoCs);
  std::map<uint256, NodeId> m_txToRelay GUARDED_BY(m_embargoCs);

  bool SendToAndRemember(NodeId relay, const uint256 &txHash);
 protected:
  boost::optional<NodeId> GetNewRelay();
  std::unordered_set<NodeId> m_unwantedRelays;
};

}  // namespace dandelion

#endif  //UNITE_DANDELION_DANDELION_H
