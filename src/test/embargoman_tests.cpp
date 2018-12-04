// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>
#include <set>

#include <p2p/embargoman.h>
#include <test/test_unite.h>
#include <uint256.h>
#include <util.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(embargoman_tests, ReducedTestingSetup)

class SideEffectsMock : public p2p::EmbargoManSideEffects {
 public:
  EmbargoTime GetNextEmbargoTime() override {
    return next_embargo_time;
  }

  bool IsEmbargoDue(EmbargoTime time) override {
    return time < now;
  }

  std::set<p2p::NodeId> GetOutboundNodes() override {
    return outbounds;
  }

  size_t RandRange(size_t max_excluding) override {
    return 0;
  }

  bool SendTxInv(p2p::NodeId node_id, const uint256 &tx_hash) override {
    const auto it = std::find(outbounds.begin(), outbounds.end(), node_id);
    if (it != outbounds.end()) {
      txs_sent_to_node[tx_hash] = node_id;
      return true;
    }
    return false;
  }

  void SendTxInvToAll(const uint256 &txHash) override {
    txs_sent_to_all.emplace(txHash);
  }

  std::set<p2p::NodeId> outbounds;
  EmbargoTime now = 0;
  EmbargoTime next_embargo_time = 10;
  std::map<uint256, p2p::NodeId> txs_sent_to_node;
  std::set<uint256> txs_sent_to_all;
};

CTransactionRef CreateNewTx() {
  CMutableTransaction tx;
  tx.vin.resize(1);

  // We want different tx hashes all the time
  static uint32_t nonce = 0;
  tx.vin[0].prevout.n = nonce;

  ++nonce;

  return MakeTransactionRef(tx);
}

uint256 CheckSendsTo(p2p::NodeId expected_relay,
                     p2p::EmbargoMan &instance,
                     const SideEffectsMock *side_effects) {
  const auto tx = CreateNewTx();
  const auto hash = tx->GetHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(*tx));

  const auto it = side_effects->txs_sent_to_node.find(hash);
  BOOST_CHECK(it != side_effects->txs_sent_to_node.end());

  BOOST_CHECK_EQUAL(expected_relay, it->second);

  BOOST_CHECK(!instance.IsEmbargoedFor(hash, expected_relay));
  BOOST_CHECK(instance.IsEmbargoedFor(hash, expected_relay + 1));

  return hash;
}

p2p::NodeId DetectRelay(p2p::EmbargoMan &instance,
                        const SideEffectsMock *side_effects) {
  const auto tx = CreateNewTx();
  const auto hash = tx->GetHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(*tx));

  const auto it = side_effects->txs_sent_to_node.find(hash);
  BOOST_CHECK(it != side_effects->txs_sent_to_node.end());
  return it->second;
}

BOOST_AUTO_TEST_CASE(test_relay_is_not_changing) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {17, 7};

  p2p::EmbargoMan instance(2, std::move(u_ptr));
  const auto relay = DetectRelay(instance, side_effects);

  for (size_t i = 0; i < 100; ++i) {
    CheckSendsTo(relay, instance, side_effects);
  }
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_disconnected) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {17};

  p2p::EmbargoMan instance(2, std::move(u_ptr));

  const auto relay1 = DetectRelay(instance, side_effects);

  side_effects->outbounds = {7};

  const auto relay2 = DetectRelay(instance, side_effects);
  BOOST_CHECK(relay1 != relay2);
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_black_hole) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {1, 2, 3, 4, 5};
  side_effects->now = 100;
  side_effects->next_embargo_time = 0;

  const auto timeouts_to_switch_relay = 4;
  p2p::EmbargoMan instance(timeouts_to_switch_relay, std::move(u_ptr));

  std::set<p2p::NodeId> banned_relays;
  for (size_t j = 0; j < side_effects->outbounds.size() - 1; ++j) {
    const auto probe_tx = CreateNewTx();
    instance.SendTransactionAndEmbargo(*probe_tx);
    const uint256 &probe_hash = probe_tx->GetHash();
    const p2p::NodeId relay_before = side_effects->txs_sent_to_node[probe_hash];
    // To reset "timeouts in a row" counter:
    instance.OnTxInv(probe_hash, relay_before + 1);

    for (size_t i = 0; i < timeouts_to_switch_relay; ++i) {
      CheckSendsTo(relay_before, instance, side_effects);
      instance.FluffPendingEmbargoes();
    }

    const p2p::NodeId relay_after = DetectRelay(instance, side_effects);
    BOOST_CHECK(relay_before != relay_after);

    const bool added = banned_relays.emplace(relay_before).second;
    BOOST_CHECK(added);
  }
}

BOOST_AUTO_TEST_CASE(change_relay_during_embargo) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  constexpr auto blackhole = 17;
  constexpr uint16_t timeouts_to_switch_relay = 2;
  side_effects->now = 100;
  side_effects->next_embargo_time = 0;

  side_effects->outbounds = {blackhole};

  p2p::EmbargoMan instance(timeouts_to_switch_relay, std::move(u_ptr));

  std::vector<uint256> blackhole_txs;
  for (size_t i = 0; i < timeouts_to_switch_relay; ++i) {
    blackhole_txs.emplace_back(CheckSendsTo(17, instance, side_effects));
  }

  // Trigger relay change by disconnecting
  side_effects->outbounds = {7, 11};
  const auto relay = DetectRelay(instance, side_effects);

  // Relay has changed but invs from previous relay should not fluff txs
  for (const auto &blackhole_tx : blackhole_txs) {
    instance.OnTxInv(blackhole_tx, blackhole);
    BOOST_CHECK(instance.IsEmbargoed(blackhole_tx));
  }

  instance.FluffPendingEmbargoes();

  // Checking that new relay is not affected by the fact that lots of
  // transactions sent to prev relay were fluffed
  BOOST_CHECK_EQUAL(relay, DetectRelay(instance, side_effects));
}

BOOST_AUTO_TEST_CASE(test_simple_embargoes) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {17};

  p2p::EmbargoMan instance(1000, std::move(u_ptr));

  const auto tx1 = CreateNewTx();
  const auto tx2 = CreateNewTx();
  const auto tx3 = CreateNewTx();

  side_effects->next_embargo_time = 10;
  instance.SendTransactionAndEmbargo(*tx1);

  side_effects->next_embargo_time = 20;
  instance.SendTransactionAndEmbargo(*tx2);

  side_effects->next_embargo_time = 30;
  instance.SendTransactionAndEmbargo(*tx3);

  BOOST_CHECK(instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx3->GetHash()));

  side_effects->now = 15;

  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(0, side_effects->txs_sent_to_all.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(0, side_effects->txs_sent_to_all.count(tx3->GetHash()));

  // Received from relay -> embargo is not lifted
  instance.OnTxInv(tx2->GetHash(), 17);

  // Received from other node -> embargo is lifted
  instance.OnTxInv(tx3->GetHash(), 1);

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(0, side_effects->txs_sent_to_all.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx3->GetHash()));

  side_effects->now = 50;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(tx3->GetHash()));
}

class EmargoManSpy : public p2p::EmbargoMan {
 public:
  EmargoManSpy(size_t timeouts_to_switch_relay,
               std::unique_ptr<p2p::EmbargoManSideEffects> side_effects)
      : EmbargoMan(timeouts_to_switch_relay, std::move(side_effects)) {}

  boost::optional<p2p::NodeId> GetNewRelay() {
    return p2p::EmbargoMan::GetNewRelay();
  }

  std::set<p2p::NodeId> &GetUnwantedRelays() {
    return p2p::EmbargoMan::m_unwanted_relays;
  }
};

BOOST_AUTO_TEST_CASE(test_unwanted_relay_filtering) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {1, 2, 3};

  EmargoManSpy spy(1000, std::move(u_ptr));

  auto &unwanted = spy.GetUnwantedRelays();
  unwanted.emplace(1);
  unwanted.emplace(3);
  unwanted.emplace(4);
  unwanted.emplace(5);
  unwanted.emplace(12);
  unwanted.emplace(10);

  BOOST_CHECK_EQUAL(2, spy.GetNewRelay().get());

  // As a side effect, GetNewRelay should trim unwanted set to only available
  // nodes
  BOOST_CHECK_EQUAL(2, unwanted.size());
  BOOST_CHECK(unwanted.count(1));
  BOOST_CHECK(unwanted.count(3));
}

BOOST_AUTO_TEST_CASE(test_child_never_fluffs_before_parent) {
  const auto side_effects = new SideEffectsMock();
  auto u_ptr = std::unique_ptr<p2p::EmbargoManSideEffects>(side_effects);

  side_effects->outbounds = {17};

  p2p::EmbargoMan instance(1000, std::move(u_ptr));

  CTransaction parent_tx;
  CMutableTransaction child_tx;

  child_tx.vin.resize(1);
  child_tx.vin[0].prevout.hash = parent_tx.GetHash();

  // Embargo Time for parent
  side_effects->next_embargo_time = 50;
  instance.SendTransactionAndEmbargo(parent_tx);

  // Embargo Time for child, less than parent
  side_effects->next_embargo_time = 10;
  instance.SendTransactionAndEmbargo(CTransaction(child_tx));

  // Set 'now' after child embargo:
  side_effects->now = 11;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK_EQUAL(0, side_effects->txs_sent_to_all.count(child_tx.GetHash()));

  // Set 'now' after parent embargo:
  side_effects->now = 51;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(child_tx.GetHash()));
  BOOST_CHECK_EQUAL(1, side_effects->txs_sent_to_all.count(parent_tx.GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()
