// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>
#include <set>

#include <embargoman.h>
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

uint256 GetNewTxHash() {
  static uint8_t counter = 0;
  assert(counter < std::numeric_limits<decltype(counter)>::max());

  return uint256(std::vector<uint8_t>(32, counter++));
}

uint256 CheckSendsTo(p2p::NodeId expectedRelay,
                     p2p::EmbargoMan &instance,
                     const SideEffectsMock *sideEffects) {
  const auto hash = GetNewTxHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(hash));

  const auto it = sideEffects->txsSentToNode.find(hash);
  BOOST_CHECK(it != sideEffects->txsSentToNode.end());

  BOOST_CHECK_EQUAL(expectedRelay, it->second);

  BOOST_CHECK(!instance.IsEmbargoedFor(hash, expectedRelay));
  BOOST_CHECK(instance.IsEmbargoedFor(hash, expectedRelay + 1));

  return hash;
}

p2p::NodeId GuessRelay(p2p::EmbargoMan &instance,
                       const SideEffectsMock *sideEffects) {
  const auto hash = GetNewTxHash();
  BOOST_CHECK(instance.SendTransactionAndEmbargo(hash));

  const auto it = sideEffects->txsSentToNode.find(hash);
  assert(it != sideEffects->txsSentToNode.end());
  return it->second;
}

BOOST_AUTO_TEST_CASE(test_relay_is_not_changing) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17, 7};

  p2p::EmbargoMan instance(2, std::move(uPtr));
  const auto relay = GuessRelay(instance, sideEffects);

  for (size_t i = 0; i < 100; ++i) {
    CheckSendsTo(relay, instance, sideEffects);
  }
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_disconnected) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17};

  p2p::EmbargoMan instance(2, std::move(uPtr));

  const auto relay1 = GuessRelay(instance, sideEffects);

  sideEffects->outbounds = {7};

  const auto relay2 = GuessRelay(instance, sideEffects);
  BOOST_CHECK(relay1 != relay2);
}

BOOST_AUTO_TEST_CASE(test_relay_is_changing_if_black_hole) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17, 7};
  sideEffects->now = 100;
  sideEffects->nextEmbargoTime = 0;

  const auto timeoutsToSwitchRelay = 4;
  p2p::EmbargoMan instance(timeoutsToSwitchRelay, std::move(uPtr));

  const auto relay1 = GuessRelay(instance, sideEffects);
  for (size_t i = 0; i < timeoutsToSwitchRelay; ++i) {
    CheckSendsTo(relay1, instance, sideEffects);
  }

  instance.FluffPendingEmbargoes();

  const auto relay2 = GuessRelay(instance, sideEffects);
  BOOST_CHECK(relay1 != relay2);
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
  const auto relay = GuessRelay(instance, sideEffects);

  // Relay has changed but invs from previous relay should not fluff txs
  for (const auto &blackholeTx : blackholeTxs) {
    instance.OnTxInv(blackholeTx, blackhole);
    BOOST_CHECK(instance.IsEmbargoed(blackholeTx));
  }

  instance.FluffPendingEmbargoes();

  // Checking that new relay is not affected by the fact that lots of
  // transactions sent to prev relay were fluffed
  BOOST_CHECK_EQUAL(relay, GuessRelay(instance, sideEffects));
}

BOOST_AUTO_TEST_CASE(test_simple_embargoes) {
  const auto sideEffects = new SideEffectsMock();
  auto uPtr = std::unique_ptr<p2p::EmbargoManSideEffects>(sideEffects);

  sideEffects->outbounds = {17};

  p2p::EmbargoMan instance(1000, std::move(uPtr));

  const auto tx1 = GetNewTxHash();
  const auto tx2 = GetNewTxHash();
  const auto tx3 = GetNewTxHash();

  sideEffects->nextEmbargoTime = 10;
  instance.SendTransactionAndEmbargo(tx1);

  sideEffects->nextEmbargoTime = 20;
  instance.SendTransactionAndEmbargo(tx2);

  sideEffects->nextEmbargoTime = 30;
  instance.SendTransactionAndEmbargo(tx3);

  BOOST_CHECK(instance.IsEmbargoed(tx1));
  BOOST_CHECK(instance.IsEmbargoed(tx2));
  BOOST_CHECK(instance.IsEmbargoed(tx3));

  sideEffects->now = 15;

  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1));
  BOOST_CHECK(instance.IsEmbargoed(tx2));
  BOOST_CHECK(instance.IsEmbargoed(tx3));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx2));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx3));

  // Received from relay -> embargo is not lifted
  instance.OnTxInv(tx2, 17);

  // Received from other node -> embargo is lifted
  instance.OnTxInv(tx3, 1);

  BOOST_CHECK(!instance.IsEmbargoed(tx1));
  BOOST_CHECK(instance.IsEmbargoed(tx2));
  BOOST_CHECK(!instance.IsEmbargoed(tx3));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1));
  BOOST_CHECK_EQUAL(0, sideEffects->txsSentToAll.count(tx2));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx3));

  sideEffects->now = 50;
  instance.FluffPendingEmbargoes();

  BOOST_CHECK(!instance.IsEmbargoed(tx1));
  BOOST_CHECK(!instance.IsEmbargoed(tx2));
  BOOST_CHECK(!instance.IsEmbargoed(tx3));

  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx1));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx2));
  BOOST_CHECK_EQUAL(1, sideEffects->txsSentToAll.count(tx3));
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

BOOST_AUTO_TEST_CASE(unwanted_relay_filtering) {
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

BOOST_AUTO_TEST_SUITE_END()
