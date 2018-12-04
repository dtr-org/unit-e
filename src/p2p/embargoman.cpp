// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/embargoman.h>

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
                                   const CTransaction &tx) {
  AssertLockHeld(m_relayCs);

  AssertLockNotHeld(m_embargoCs);

  const auto txHash = tx.GetHash();
  const auto sent = m_sideEffects->SendTxInv(relay, tx.GetHash());

  if (sent) {
    if (m_relay != relay) {
      m_timeoutsInARow = 0;
    }

    m_relay = relay;
    const EmbargoTime embargoTime = GetEmbargoTime(tx);

    LOCK(m_embargoCs);
    m_embargoes.emplace(txHash, Embargo(relay, embargoTime));
    m_embargoToTx.emplace(embargoTime, txHash);

    return true;
  }

  m_unwantedRelays.emplace(relay);
  m_relay = boost::none;

  return false;
}

bool EmbargoMan::SendTransactionAndEmbargo(const CTransaction &tx) {
  LOCK(m_relayCs);

  bool sent = false;
  if (m_relay) {
    sent = SendToAndRemember(m_relay.get(), tx);
  }

  if (!sent) {
    const auto newRelay = GetNewRelay();
    if (newRelay) {
      sent = SendToAndRemember(newRelay.get(), tx);
    }
  }

  if (sent) {
    LogPrint(BCLog::NET, "Embargoman: tx %s is sent to peer=%d.\n",
             tx.GetHash().GetHex(),
             m_relay.get());
  } else {
    LogPrint(BCLog::NET, "Embargoman: failed to send tx %s.\n",
             tx.GetHash().GetHex());
  }

  return sent;
}

void EmbargoMan::FluffPendingEmbargoes() {
  LOCK(m_relayCs);

  std::vector<uint256> txsToFluff;

  {
    LOCK(m_embargoCs);

    while (!m_embargoToTx.empty()) {
      const uint256 txHash = m_embargoToTx.begin()->second;
      const EmbargoTime embargoTime = m_embargoToTx.begin()->first;

      if (!m_sideEffects->IsEmbargoDue(embargoTime)) {
        break;
      }

      m_embargoToTx.erase(m_embargoToTx.begin());

      const auto it = m_embargoes.find(txHash);

      if (it == m_embargoes.end()) {
        // This transaction was earlier Inv'ed from non-relay
        m_timeoutsInARow = 0;
        continue;
      }

      const NodeId usedRelay = it->second.relay;
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
      m_embargoes.erase(it);

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

  return m_embargoes.find(txHash) != m_embargoes.end();
}

bool EmbargoMan::IsEmbargoedFor(const uint256 &txHash,
                                NodeId node) const {
  LOCK(m_embargoCs);

  const auto it = m_embargoes.find(txHash);
  if (it == m_embargoes.end()) {
    return false;
  }

  const NodeId relay = it->second.relay;

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

    const auto it = m_embargoes.find(txHash);
    if (it == m_embargoes.end()) {
      return;
    }

    const NodeId usedRelay = it->second.relay;
    if (from == usedRelay) {
      // From spec:
      // If v’s timer expires before it receives an INV for the transaction from a
      // node other than the Dandelion relay, it starts the fluff phase.
      return;
    }

    m_embargoes.erase(it);

    LogPrint(BCLog::NET, "Embargo is lifted for tx: %s. Fluffing\n",
             txHash.GetHex());
  }

  AssertLockNotHeld(m_embargoCs);
  m_sideEffects->SendTxInvToAll(txHash);
}

EmbargoMan::EmbargoTime EmbargoMan::GetEmbargoTime(const CTransaction &tx) {
  EmbargoTime embargoTime = m_sideEffects->GetNextEmbargoTime();

  LOCK(m_embargoCs);

  for (const CTxIn &input : tx.vin) {
    const uint256 &parentHash = input.prevout.hash;

    const auto it = m_embargoes.find(parentHash);
    if (it == m_embargoes.end()) {
      continue;
    }

    // If a child transaction fluffs before parent - this will cause us to
    // relay an orphan. This significantly slows down propagation of our
    // transaction since our neighbors will fail to receive parent from us - and
    // they won't try to download it again in next 2 minutes
    embargoTime = std::max(embargoTime, it->second.embargoTime);
  }

  return embargoTime;
}

EmbargoMan::Embargo::Embargo(NodeId relay, EmbargoTime embargoTime)
    : relay(relay), embargoTime(embargoTime) {}

}  // namespace p2p
