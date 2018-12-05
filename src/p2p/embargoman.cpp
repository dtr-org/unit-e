// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <p2p/embargoman.h>

namespace p2p {

boost::optional<NodeId> EmbargoMan::GetNewRelay() {
  LOCK(m_relay_cs);

  // Get all available outbound connections
  std::set<NodeId> outbounds = m_side_effects->GetOutboundNodes();

  // Some of unwanted nodes might have been disconnected,
  // filter those that are not present in outbounds
  for (auto it = m_unwanted_relays.begin(); it != m_unwanted_relays.end();) {
    if (outbounds.find(*it) == outbounds.end()) {
      it = m_unwanted_relays.erase(it);
    } else {
      ++it;
    }
  }

  // Filter unwanted nodes
  for (const NodeId &unwanted : m_unwanted_relays) {
    outbounds.erase(unwanted);
  }

  if (outbounds.empty()) {
    return boost::none;
  }

  auto relay_it = outbounds.begin();
  const auto offset = m_side_effects->RandRange(outbounds.size());
  std::advance(relay_it, offset);
  return *relay_it;
}

bool EmbargoMan::SendToAndRemember(NodeId relay,
                                   const CTransaction &tx) {
  AssertLockHeld(m_relay_cs);

  AssertLockNotHeld(m_embargo_cs);

  const auto tx_hash = tx.GetHash();
  const auto sent = m_side_effects->SendTxInv(relay, tx.GetHash());

  if (sent) {
    if (m_relay != relay) {
      m_timeouts_in_a_row = 0;
    }

    m_relay = relay;
    const EmbargoTime embargo_time = GetEmbargoTime(tx);

    LOCK(m_embargo_cs);
    m_embargoes.emplace(tx_hash, Embargo(relay, embargo_time));
    m_embargo_to_tx.emplace(embargo_time, tx_hash);

    return true;
  }

  m_unwanted_relays.emplace(relay);
  m_relay = boost::none;

  return false;
}

bool EmbargoMan::SendTransactionAndEmbargo(const CTransaction &tx) {
  LOCK(m_relay_cs);

  bool sent = false;
  if (m_relay) {
    sent = SendToAndRemember(m_relay.get(), tx);
  }

  if (!sent) {
    const auto new_relay = GetNewRelay();
    if (new_relay) {
      sent = SendToAndRemember(new_relay.get(), tx);
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
  LOCK(m_relay_cs);

  std::vector<uint256> txs_to_fluff;

  {
    LOCK(m_embargo_cs);

    while (!m_embargo_to_tx.empty()) {
      const uint256 tx_hash = m_embargo_to_tx.begin()->second;
      const EmbargoTime embargo_time = m_embargo_to_tx.begin()->first;

      if (!m_side_effects->IsEmbargoDue(embargo_time)) {
        break;
      }

      m_embargo_to_tx.erase(m_embargo_to_tx.begin());

      const auto it = m_embargoes.find(tx_hash);

      if (it == m_embargoes.end()) {
        // This transaction was earlier Inv'ed from non-relay
        m_timeouts_in_a_row = 0;
        continue;
      }

      const NodeId used_relay = it->second.relay;
      if (m_relay == used_relay) {
        ++m_timeouts_in_a_row;
        if (m_timeouts_in_a_row >= m_timeouts_to_switch_relay) {
          LogPrint(BCLog::NET, "Embargo timer fired %d times in a row. Changing relay.\n",
                   m_timeouts_in_a_row);

          m_unwanted_relays.emplace(m_relay.get());
          m_relay = boost::none;
        }
      }

      LogPrint(BCLog::NET, "Embargo timer expired. Fluffing: %s.\n", tx_hash.GetHex());
      m_embargoes.erase(it);

      txs_to_fluff.emplace_back(tx_hash);
    }
  }

  AssertLockNotHeld(m_embargo_cs);
  for (const uint256 &tx : txs_to_fluff) {
    m_side_effects->SendTxInvToAll(tx);
  }
}

bool EmbargoMan::IsEmbargoed(const uint256 &tx_hash) const {
  LOCK(m_embargo_cs);

  return m_embargoes.find(tx_hash) != m_embargoes.end();
}

bool EmbargoMan::IsEmbargoedFor(const uint256 &tx_hash,
                                NodeId node) const {
  LOCK(m_embargo_cs);

  const auto it = m_embargoes.find(tx_hash);
  if (it == m_embargoes.end()) {
    return false;
  }

  const NodeId relay = it->second.relay;

  return relay != node;
}

EmbargoMan::EmbargoMan(size_t timeouts_to_switch_relay,
                       std::unique_ptr<EmbargoManSideEffects> side_effects)
    : m_timeouts_to_switch_relay(timeouts_to_switch_relay),
      m_side_effects(std::move(side_effects)) {
  LogPrint(BCLog::NET, "EmbargoMan is created.\n");
}

void EmbargoMan::OnTxInv(const uint256 &tx_hash, NodeId from) {
  {
    LOCK(m_embargo_cs);

    const auto it = m_embargoes.find(tx_hash);
    if (it == m_embargoes.end()) {
      return;
    }

    const NodeId used_relay = it->second.relay;
    if (from == used_relay) {
      // From spec:
      // If v’s timer expires before it receives an INV for the transaction from a
      // node other than the Dandelion relay, it starts the fluff phase.
      return;
    }

    m_embargoes.erase(it);

    LogPrint(BCLog::NET, "Embargo is lifted for tx: %s. Fluffing\n",
             tx_hash.GetHex());
  }

  AssertLockNotHeld(m_embargo_cs);
  m_side_effects->SendTxInvToAll(tx_hash);
}

EmbargoMan::EmbargoTime EmbargoMan::GetEmbargoTime(const CTransaction &tx) {
  EmbargoTime embargo_time = m_side_effects->GetNextEmbargoTime();

  LOCK(m_embargo_cs);

  for (const CTxIn &input : tx.vin) {
    const uint256 &parent_hash = input.prevout.hash;

    const auto it = m_embargoes.find(parent_hash);
    if (it == m_embargoes.end()) {
      continue;
    }

    // If a child transaction fluffs before parent - this will cause us to
    // relay an orphan. This significantly slows down propagation of our
    // transaction since our neighbors will fail to receive parent from us - and
    // they won't try to download it again in next 2 minutes
    embargo_time = std::max(embargo_time, it->second.embargo_time);
  }

  return embargo_time;
}

EmbargoMan::Embargo::Embargo(NodeId relay, EmbargoTime embargo_time)
    : relay(relay), embargo_time(embargo_time) {}

}  // namespace p2p
