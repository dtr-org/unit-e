// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <embargoman.h>

namespace p2p {

boost::optional<NodeId> EmbargoMan::GetNewRelay() {
  LOCK(m_relayCs);

  // Get all available outbound connections
  std::set<NodeId> outbounds = m_sideEffects->GetOutboundNodes();

  // Some of unwanted nodes might have disconnected,
  // filter those that are not present in outbounds
  for (auto it = m_unwantedRelays.begin(); it != m_unwantedRelays.end();) {
    if (outbounds.find(*it) == outbounds.end()) {
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

bool EmbargoMan::SendToAndRemember(NodeId relay,
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

bool EmbargoMan::SendTransactionAndEmbargo(const uint256 &txHash) {
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
    LogPrint(BCLog::NET, "Embargoman: tx %s is sent to peer=%d.\n", txHash.GetHex(),
             m_relay.get());
  } else {
    LogPrint(BCLog::NET, "Embargoman: failed to send tx %s.\n", txHash.GetHex());
  }

  return sent;
}

void EmbargoMan::FluffPendingEmbargoes() {
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
          LogPrint(BCLog::NET, "Embargo timer fired %d times in a row. Changing relay.\n",
                   m_timeoutsInARow);

          m_unwantedRelays.emplace(m_relay.get());
          m_relay = boost::none;
        }
      }

      LogPrint(BCLog::NET, "Embargo timer expired. Fluffing: %s.\n", txHash.GetHex());
      m_txToRelay.erase(it);

      txsToFluff.emplace_back(txHash);
    }
  }

  AssertLockNotHeld(m_embargoCs);
  for (const uint256 &tx : txsToFluff) {
    m_sideEffects->SendTxInvToAll(tx);
  }
}

bool EmbargoMan::IsEmbargoed(const uint256 &txHash) const {
  LOCK(m_embargoCs);

  return m_txToRelay.find(txHash) != m_txToRelay.end();
}

bool EmbargoMan::IsEmbargoedFor(const uint256 &txHash,
                                NodeId node) const {
  LOCK(m_embargoCs);

  const auto it = m_txToRelay.find(txHash);
  if (it == m_txToRelay.end()) {
    return false;
  }

  const NodeId relay = it->second;

  return relay != node;
}

EmbargoMan::EmbargoMan(size_t timeoutsToSwitchRelay,
                       std::unique_ptr<EmbargoManSideEffects> sideEffects)
    : m_timeoutsToSwitchRelay(timeoutsToSwitchRelay),
      m_sideEffects(std::move(sideEffects)) {
  LogPrint(BCLog::NET, "EmbargoMan is created.\n");
}

void EmbargoMan::OnTxInv(const uint256 &txHash, NodeId from) {
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

    LogPrint(BCLog::NET, "Embargo is lifted for tx: %s. Fluffing\n",
             txHash.GetHex());
  }

  AssertLockNotHeld(m_embargoCs);
  m_sideEffects->SendTxInvToAll(txHash);
}

}  // namespace network
