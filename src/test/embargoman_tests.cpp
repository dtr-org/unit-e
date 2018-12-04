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
    return nextEmbargoTime;
  }

  bool IsEmbargoDue(EmbargoTime time) override {
    return time < now;
  }

  std::set<p2p::NodeId> GetOutboundNodes() override {
    return outbounds;
  }

  size_t RandRange(size_t maxExcluding) override {
    return 0;
  }

  bool SendTxInv(p2p::NodeId nodeId, const uint256 &txHash) override {
    const auto it = std::find(outbounds.begin(), outbounds.end(), nodeId);
    if (it != outbounds.end()) {
      txsSentToNode[txHash] = nodeId;
      return true;
    }
    return false;
  }

  void SendTxInvToAll(const uint256 &txHash) override {
    txsSentToAll.emplace(txHash);
  }

  std::set<p2p::NodeId> outbounds;
  EmbargoTime now = 0;
  EmbargoTime nextEmbargoTime = 10;
  std::map<uint256, p2p::NodeId> txsSentToNode;
  std::set<uint256> txsSentToAll;
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

uint256 CheckSendsTo(p2p::NodeId expectedRelay,
                     p2p::EmbargoMan &instance,
                     const SideEffectsMock *sideEffects) {
  const auto tx = CreateNewTx();
  const auto hash = tx->GetHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(*tx));

  const auto it = sideEffects->txsSentToNode.find(hash);
  BOOST_CHECK(it != sideEffects->txsSentToNode.end());

  BOOST_CHECK_EQUAL(expectedRelay, it->second);

  BOOST_CHECK(!instance.IsEmbargoedFor(hash, expectedRelay));
  BOOST_CHECK(instance.IsEmbargoedFor(hash, expectedRelay + 1));

  return hash;
}

p2p::NodeId DetectRelay(p2p::EmbargoMan &instance,
                        const SideEffectsMock *sideEffects) {
  const auto tx = CreateNewTx();
  const auto hash = tx->GetHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(*tx));

  const auto it = sideEffects->txsSentToNode.find(hash);
  BOOST_CHECK(it != sideEffects->txsSentToNode.end());
  return it->second;
}

BOOST_AUTO_TEST_CASE(test_relay_is_not_changing) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17, 7};

  p2p::EmbargoMan instance(2, std::move(uPtr));
  const auto relay = DetectRelay(instance, sideEffects);

  for (size_t i = 0; i < 100; ++i) {
    CheckSendsTo(relay, instance, sideEffects);
  }
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_disconnected) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17};

  p2p::EmbargoMan instance(2, std::move(uPtr));

  const auto relay1 = DetectRelay(instance, sideEffects);

  sideEffects->outbounds = {7};

  const auto relay2 = DetectRelay(instance, sideEffects);
  BOOST_CHECK(relay1 != relay2);
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_black_hole) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {1, 2, 3, 4, 5};
  sideEffects->now = 100;
  sideEffects->nextEmbargoTime = 0;

  const auto timeoutsToSwitchRelay = 4;
  p2p::EmbargoMan instance(timeoutsToSwitchRelay, std::move(uPtr));

  std::set<p2p::NodeId> bannedRelays;
  for (size_t j = 0; j < sideEffects->outbounds.size() - 1; ++j) {
    const auto probeTx = CreateNewTx();
    instance.SendTransactionAndEmbargo(*probeTx);
    const p2p::NodeId relayBefore = sideEffects->txsSentToNode[probeTx->GetHash()];
    // To reset "timeouts in a row" counter:
    instance.OnTxInv(probeTx->GetHash(), relayBefore + 1);

    for (size_t i = 0; i < timeoutsToSwitchRelay; ++i) {
      CheckSendsTo(relayBefore, instance, sideEffects);
      instance.FluffPendingEmbargoes();
    }

    const p2p::NodeId relayAfter = DetectRelay(instance, sideEffects);
    BOOST_CHECK(relayBefore != relayAfter);

    const bool added = bannedRelays.emplace(relayBefore).second;
    BOOST_CHECK(added);
  }
}

BOOST_AUTO_TEST_CASE(change_relay_during_embargo) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  constexpr auto blackhole = 17;
  constexpr uint16_t timeoutsToSwitchRelay = 2;
  sideEffects->now = 100;
  sideEffects->nextEmbargoTime = 0;

  sideEffects->outbounds = {blackhole};

  p2p::EmbargoMan instance(timeoutsToSwitchRelay, std::move(uPtr));

  std::vector<uint256> blackholeTxs;
  for (size_t i = 0; i < timeoutsToSwitchRelay; ++i) {
    blackholeTxs.emplace_back(CheckSendsTo(17, instance, sideEffects));
  }

  // Trigger relay change by disconnecting
  sideEffects->outbounds = {7, 11};
  const auto relay = DetectRelay(instance, sideEffects);

  // Relay has changed but invs from previous relay should not fluff txs
  for (const auto &blackholeTx : blackholeTxs) {
    instance.OnTxInv(blackholeTx, blackhole);
    BOOST_CHECK(instance.IsEmbargoed(blackholeTx));
  }

  instance.FluffPendingEmbargoes();

  // Checking that new relay is not affected by the fact that lots of
  // transactions sent to prev relay were fluffed
  BOOST_CHECK_EQUAL(relay, DetectRelay(instance, sideEffects));
}

BOOST_AUTO_TEST_CASE(test_simple_embargoes) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17};

  p2p::EmbargoMan instance(1000, std::move(uPtr));

  const auto tx1 = CreateNewTx();
  const auto tx2 = CreateNewTx();
  const auto tx3 = CreateNewTx();

  sideEffects->nextEmbargoTime = 10;
  instance.SendTransactionAndEmbargo(*tx1);

  sideEffects->nextEmbargoTime = 20;
  instance.SendTransactionAndEmbargo(*tx2);

  sideEffects->nextEmbargoTime = 30;
  instance.SendTransactionAndEmbargo(*tx3);

  BOOST_CHECK(instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx3->GetHash()));

  sideEffects->now = 15;

  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx3->GetHash()));

  // Received from relay -> embargo is not lifted
  instance.OnTxInv(tx2->GetHash(), 17);

  // Received from other node -> embargo is lifted
  instance.OnTxInv(tx3->GetHash(), 1);

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx3->GetHash()));

  sideEffects->now = 50;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx2->GetHash()));
  BOOST_CHECK(!instance.IsEmbargoed(tx3->GetHash()));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1->GetHash()));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx2->GetHash()));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx3->GetHash()));
}

class EmargoManSpy : public p2p::EmbargoMan {
 public:
  EmargoManSpy(size_t timeoutsToSwitchRelay,
               std::unique_ptr<p2p::EmbargoManSideEffects> sideEffects)
      : EmbargoMan(timeoutsToSwitchRelay, std::move(sideEffects)) {}

  boost::optional<p2p::NodeId> GetNewRelay() {
    return p2p::EmbargoMan::GetNewRelay();
  }

  std::set<p2p::NodeId> &GetUnwantedRelays() {
    return p2p::EmbargoMan::m_unwantedRelays;
  }
};

BOOST_AUTO_TEST_CASE(test_unwanted_relay_filtering) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {1, 2, 3};

  EmargoManSpy spy(1000, std::move(uPtr));

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
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17};

  p2p::EmbargoMan instance(1000, std::move(uPtr));

  CTransaction parentTx;
  CMutableTransaction childTx;

  childTx.vin.resize(1);
  childTx.vin[0].prevout.hash = parentTx.GetHash();

  // Embargo Time for parent
  sideEffects->nextEmbargoTime = 50;
  instance.SendTransactionAndEmbargo(parentTx);

  // Embargo Time for child, less than parent
  sideEffects->nextEmbargoTime = 10;
  instance.SendTransactionAndEmbargo(CTransaction(childTx));

  // Set 'now' after child embargo:
  sideEffects->now = 11;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(childTx.GetHash()));

  // Set 'now' after parent embargo:
  sideEffects->now = 51;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(childTx.GetHash()));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(parentTx.GetHash()));
}

BOOST_AUTO_TEST_SUITE_END()
