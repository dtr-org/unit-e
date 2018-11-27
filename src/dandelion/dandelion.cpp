// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dandelion/dandelion.h>

namespace dandelion {

boost::optional<NodeId> DandelionLite::GetNewRelay() {
  LOCK(m_main_cs);

  // Get all available outbound connections
  const auto outbounds = m_sideEffects->GetOutboundNodes();
  auto outboundsSet = std::unordered_set<NodeId>(outbounds.begin(), outbounds.end());

  // Some of unwanted nodes might have disconnected,
  // Filter those that do not present in outboundsSet
  for (auto it = m_unwantedRelays.begin(); it != m_unwantedRelays.end();) {
    if (outboundsSet.find(*it) == outboundsSet.end()) {
      it = m_unwantedRelays.erase(it);
    } else {
      ++it;
    }
  }

  // Filter unwanted nodes
  for (const auto &unwanted : m_unwantedRelays) {
    outboundsSet.erase(unwanted);
  }

  if (outboundsSet.empty()) {
    return boost::none;
  }

  auto relayIt = outboundsSet.begin();
  const auto offset = m_sideEffects->RandRange(outboundsSet.size());
  std::advance(relayIt, offset);
  return *relayIt;
}

bool DandelionLite::SendToAndRemember(boost::optional<NodeId> relay,
                                      const uint256 &txHash) {
  LOCK(m_main_cs);

  if (!relay) {
    return false;
  }

  AssertLockNotHeld(m_embargo_cs);
  const auto sent = m_sideEffects->SendTxInv(relay.value(), txHash);

  if (sent) {
    m_relay = relay;
    const auto embargo = m_sideEffects->GetNextEmbargoTime();

    LOCK(m_embargo_cs);
    m_txToRelay.emplace(txHash, relay.value());
    m_embargoToTx.emplace(embargo, txHash);

    return true;
  }

  m_unwantedRelays.emplace(relay.value());
  m_relay = boost::none;

  return false;
}

bool DandelionLite::SendTransaction(const uint256 &txHash) {
  LOCK(m_main_cs);

  bool sent = SendToAndRemember(m_relay, txHash);

  if (!sent) {
    const auto newRelay = GetNewRelay();
    sent = SendToAndRemember(newRelay, txHash);
  }

  if (sent) {
    LogPrintf("Dandelion tx %s is sent to peer=%d.\n", txHash.GetHex(),
              m_relay.value());
  } else {
    LogPrintf("Failed to send dandelion tx %s.\n", txHash.GetHex());
  }

  return sent;
}

void DandelionLite::FluffPendingEmbargoes() {
  LOCK(m_main_cs);

  std::vector<uint256> txsToFluff;

  {
    LOCK(m_embargo_cs);

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

      const auto usedRelay = it->second;
      if (m_relay == usedRelay) {
        ++m_timeoutsInARow;
        if (m_timeoutsInARow >= m_timeoutsToSwitchRelay) {
          LogPrintf("Dandelion relay failed %d times in a row. Changing.\n",
                    m_timeoutsInARow);

          m_unwantedRelays.emplace(m_relay.value());
          m_relay = boost::none;
        }
      }

      LogPrintf("Dandelion is fluffing embargoed tx: %s.\n", txHash.GetHex());
      m_txToRelay.erase(it);

      txsToFluff.emplace_back(txHash);
    }
  }

  AssertLockNotHeld(m_embargo_cs);
  for (const auto &tx : txsToFluff) {
    m_sideEffects->SendTxInvToAll(tx);
  }
}

bool DandelionLite::IsEmbargoed(const uint256 &txHash) const {
  LOCK(m_embargo_cs);

  return m_txToRelay.find(txHash) != m_txToRelay.end();
}

bool DandelionLite::IsEmbargoedFor(const uint256 &txHash,
                                   dandelion::NodeId node) const {
  LOCK(m_embargo_cs);

  const auto it = m_txToRelay.find(txHash);
  if (it == m_txToRelay.end()) {
    return false;
  }

  const auto relay = it->second;

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
    LOCK(m_embargo_cs);

    const auto it = m_txToRelay.find(txHash);
    if (it == m_txToRelay.end()) {
      return;
    }

    const auto usedRelay = it->second;
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

  AssertLockNotHeld(m_embargo_cs);
  m_sideEffects->SendTxInvToAll(txHash);
}

}  // namespace dandelion
