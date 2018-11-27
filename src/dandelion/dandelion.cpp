// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dandelion/dandelion.h>

namespace dandelion {

boost::optional<NodeId> DandelionLite::GetNewRelay() {
  LOCK(m_relayCs);

  // Get all available outbound connections
  std::unordered_set<NodeId> outbounds = m_sideEffects->GetOutboundNodes();

  // Some of unwanted nodes might have disconnected,
  // filter those that are not present in outbounds
  for (auto it = m_unwantedRelays.begin(); it != m_unwantedRelays.end();) {
    if (outbounds.find(*it) == outbounds.end()) {
      // Erase returns iterator following the last removed element.
      // ...Other iterators and references are not invalidated.
      // https://en.cppreference.com/w/cpp/container/unordered_map/erase
      it = m_unwantedRelays.erase(it);
    } else {
      ++it;
    }
  }

  // Filter unwanted nodes
  for (const NodeId &unwanted : m_unwantedRelays) {
    outbounds.erase(unwanted);
  }

  if (outbounds.empty()) {
    return boost::none;
  }

  auto relayIt = outbounds.begin();
  const auto offset = m_sideEffects->RandRange(outbounds.size());
  std::advance(relayIt, offset);
  return *relayIt;
}

bool DandelionLite::SendToAndRemember(NodeId relay,
                                      const uint256 &txHash) {
  AssertLockHeld(m_relayCs);

  AssertLockNotHeld(m_embargoCs);
  const auto sent = m_sideEffects->SendTxInv(relay, txHash);

  if (sent) {
    m_relay = relay;
    const auto embargo = m_sideEffects->GetNextEmbargoTime();

    LOCK(m_embargoCs);
    m_txToRelay.emplace(txHash, relay);
    m_embargoToTx.emplace(embargo, txHash);

    return true;
  }

  m_unwantedRelays.emplace(relay);
  m_relay = boost::none;

  return false;
}

bool DandelionLite::SendTransaction(const uint256 &txHash) {
  LOCK(m_relayCs);

  bool sent = false;
  if (m_relay) {
    sent = SendToAndRemember(m_relay.get(), txHash);
  }

  if (!sent) {
    const auto newRelay = GetNewRelay();
    if (newRelay) {
      sent = SendToAndRemember(newRelay.get(), txHash);
    }
  }

  if (sent) {
    LogPrintf("Dandelion tx %s is sent to peer=%d.\n", txHash.GetHex(),
              m_relay.get());
  } else {
    LogPrintf("Failed to send dandelion tx %s.\n", txHash.GetHex());
  }

  return sent;
}

void DandelionLite::FluffPendingEmbargoes() {
  LOCK(m_relayCs);

  std::vector<uint256> txsToFluff;

  {
    LOCK(m_embargoCs);

    while (!m_embargoToTx.empty()) {
      const auto txHash = m_embargoToTx.begin()->second;
      const auto embargoTime = m_embargoToTx.begin()->first;

      if (!m_sideEffects->IsEmbargoDue(embargoTime)) {
        break;
      }

      m_embargoToTx.erase(m_embargoToTx.begin());

      const auto it = m_txToRelay.find(txHash);

      if (it == m_txToRelay.end()) {
        // This transaction was earlier Inv'ed from non-relay
        m_timeoutsInARow = 0;
        continue;
      }

      const NodeId usedRelay = it->second;
      if (m_relay == usedRelay) {
        ++m_timeoutsInARow;
        if (m_timeoutsInARow >= m_timeoutsToSwitchRelay) {
          LogPrintf("Dandelion relay failed %d times in a row. Changing.\n",
                    m_timeoutsInARow);

          m_unwantedRelays.emplace(m_relay.get());
          m_relay = boost::none;
        }
      }

      LogPrintf("Dandelion is fluffing embargoed tx: %s.\n", txHash.GetHex());
      m_txToRelay.erase(it);

      txsToFluff.emplace_back(txHash);
    }
  }

  AssertLockNotHeld(m_embargoCs);
  for (const uint256 &tx : txsToFluff) {
    m_sideEffects->SendTxInvToAll(tx);
  }
}

bool DandelionLite::IsEmbargoed(const uint256 &txHash) const {
  LOCK(m_embargoCs);

  return m_txToRelay.find(txHash) != m_txToRelay.end();
}

bool DandelionLite::IsEmbargoedFor(const uint256 &txHash,
                                   NodeId node) const {
  LOCK(m_embargoCs);

  const auto it = m_txToRelay.find(txHash);
  if (it == m_txToRelay.end()) {
    return false;
  }

  const NodeId relay = it->second;

  return relay != node;
}

DandelionLite::DandelionLite(size_t timeoutsToSwitchRelay,
                             std::unique_ptr<SideEffects> sideEffects)
    : m_timeoutsToSwitchRelay(timeoutsToSwitchRelay),
      m_sideEffects(std::move(sideEffects)) {
  LogPrintf("Dandelion-lite is created.\n");
}

void DandelionLite::OnTxInv(const uint256 &txHash, NodeId from) {
  {
    LOCK(m_embargoCs);

    const auto it = m_txToRelay.find(txHash);
    if (it == m_txToRelay.end()) {
      return;
    }

    const NodeId usedRelay = it->second;
    if (from == usedRelay) {
      // From spec:
      // If v’s timer expires before it receives an INV for the transaction from a
      // node other than the Dandelion relay, it starts the fluff phase.
      return;
    }

    m_txToRelay.erase(it);

    LogPrintf("Dandelion embargo is lifted for tx: %s. Fluffing\n",
              txHash.GetHex());
  }

  AssertLockNotHeld(m_embargoCs);
  m_sideEffects->SendTxInvToAll(txHash);
}

}  // namespace dandelion
