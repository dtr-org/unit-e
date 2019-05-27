// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <map>
#include <set>

#include <blockencodings.h>
#include <consensus/ltor.h>
#include <p2p/embargoman.h>
#include <p2p/graphene.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <test/test_unite.h>
#include <uint256.h>
#include <util/system.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(grapheneblock_tests, ReducedTestingSetup)

CTransactionRef CreateCoinbase() {
  CMutableTransaction coinbase;
  coinbase.vin.resize(1);
  coinbase.SetType(TxType::COINBASE);

  return MakeTransactionRef(std::move(coinbase));
}

CTransactionRef CreateTx(size_t seed) {
  // Although this is not very realistic transaction,
  // it is created very fast. And graphene does not really need real transaction,
  // only different hashes
  CMutableTransaction mut_tx;
  mut_tx.vout.resize(1);
  mut_tx.vout[0].nValue = seed;

  return MakeTransactionRef(std::move(mut_tx));
}

class MempoolMock : public ::TxPool {
 public:
  size_t GetTxCount() const override {
    return txs.size();
  }

  std::vector<CTransactionRef> GetTxs() const override {
    return txs;
  }

  std::vector<CTransactionRef> txs;
};

void CheckBlocksEqual(const CBlock &expected,
                      const CBlock &actual) {

  const CBlockHeader expected_header = expected.GetBlockHeader();
  const CBlockHeader actual_header = actual.GetBlockHeader();

  BOOST_CHECK_EQUAL(actual_header.GetHash(), expected_header.GetHash());

  BOOST_CHECK_EQUAL(actual.vtx.size(), expected.vtx.size());

  for (size_t i = 0; i < expected.vtx.size(); ++i) {
    BOOST_CHECK_EQUAL(actual.vtx[i]->GetHash(), expected.vtx[i]->GetHash());
  }
}

void CheckReconstructsBack(const CBlock &original,
                           const MempoolMock &sender_mempool,
                           const MempoolMock &receiver_mempool,
                           FastRandomContext &random) {

  const auto maybe_graphene =
      p2p::CreateGrapheneBlock(original, sender_mempool.GetTxCount(),
                               receiver_mempool.GetTxCount(), random);

  BOOST_REQUIRE(maybe_graphene);
  const p2p::GrapheneBlock graphene = maybe_graphene.get();

  p2p::GrapheneBlockReconstructor reconstructor(graphene, receiver_mempool);

  CBlock reconstructed = reconstructor.ReconstructLTOR();

  CheckBlocksEqual(original, reconstructed);
}

BOOST_AUTO_TEST_CASE(coinbase_only) {
  FastRandomContext random(true);
  CBlock block;
  block.vtx.emplace_back(CreateCoinbase());

  MempoolMock sender_mempool;
  MempoolMock receiver_mempool;

  CheckReconstructsBack(block, sender_mempool, receiver_mempool, random);
}

BOOST_AUTO_TEST_CASE(exact_mempools) {
  FastRandomContext random(true);
  CBlock block;
  block.vtx.emplace_back(CreateCoinbase());

  const auto tx1 = CreateTx(0);
  const auto tx2 = CreateTx(1);

  block.vtx.emplace_back(tx1);
  block.vtx.emplace_back(tx2);

  MempoolMock sender_mempool;
  MempoolMock receiver_mempool;
  receiver_mempool.txs = {tx1, tx2};

  CheckReconstructsBack(block, sender_mempool, receiver_mempool, random);
}

BOOST_AUTO_TEST_CASE(different_mempools_but_the_same_size) {
  FastRandomContext random(true);
  CBlock block;
  block.vtx.emplace_back(CreateCoinbase());

  const auto tx1 = CreateTx(0);
  const auto tx2 = CreateTx(1);
  const auto tx3 = CreateTx(2);

  block.vtx.emplace_back(tx1);
  block.vtx.emplace_back(tx2);

  MempoolMock sender_mempool;
  MempoolMock receiver_mempool;
  receiver_mempool.txs = {tx1, tx2, tx3};

  CheckReconstructsBack(block, sender_mempool, receiver_mempool, random);
}

BOOST_AUTO_TEST_CASE(thousands_of_txs) {
  FastRandomContext random(true);
  CBlock block;
  constexpr size_t SENDER_TXS = 100000;
  constexpr size_t RECEIVER_TXS = 100000;
  constexpr size_t COMMON_TXS = 400000;

  block.vtx.emplace_back(CreateCoinbase());

  std::vector<CTransactionRef> sender_only_txs;
  std::vector<CTransactionRef> receiever_only_txs;
  std::vector<CTransactionRef> common_txs;
  size_t seed = 0;

  for (size_t i = 0; i < SENDER_TXS; ++i) {
    const CTransactionRef &tx = CreateTx(seed++);
    sender_only_txs.emplace_back(tx);
    block.vtx.emplace_back(tx);
  }

  MempoolMock receiver_mempool;

  for (size_t i = 0; i < COMMON_TXS; ++i) {
    const CTransactionRef &tx = CreateTx(seed++);
    common_txs.emplace_back(tx);
    block.vtx.emplace_back(tx);
    receiver_mempool.txs.emplace_back(tx);
  }

  for (size_t i = 0; i < RECEIVER_TXS; ++i) {
    const CTransactionRef &tx = CreateTx(seed++);
    receiever_only_txs.emplace_back(tx);
    receiver_mempool.txs.emplace_back(tx);
  }

  ltor::SortTransactions(block.vtx);

  BOOST_REQUIRE(block.vtx.size() == (SENDER_TXS + COMMON_TXS) + 1);

  const auto maybe_graphene =
      p2p::CreateGrapheneBlock(block, SENDER_TXS, receiver_mempool.GetTxCount(),
                               random);

  BOOST_REQUIRE(maybe_graphene);

  const p2p::GrapheneBlock graphene = maybe_graphene.get();

  p2p::GrapheneBlockReconstructor reconstructor(graphene, receiver_mempool);

  BOOST_CHECK_EQUAL(reconstructor.GetState(), +p2p::GrapheneDecodeState::NEED_MORE_TXS);

  p2p::GrapheneHasher hasher(graphene.header, graphene.nonce);
  std::set<uint64_t> must_be_missing;
  for (const auto &tx : sender_only_txs) {
    must_be_missing.emplace(hasher.GetShortHash(*tx));
  }

  BOOST_CHECK(must_be_missing == reconstructor.GetMissingShortTxHashes());

  reconstructor.AddMissingTxs(sender_only_txs);
  BOOST_CHECK_EQUAL(reconstructor.GetState(), +p2p::GrapheneDecodeState::HAS_ALL_TXS);
  CBlock reconstructed = reconstructor.ReconstructLTOR();

  CheckBlocksEqual(block, reconstructed);
}

size_t RandRange(size_t min_incl, size_t max_incl, FastRandomContext &random) {
  BOOST_REQUIRE(min_incl <= max_incl);
  return random.randrange(max_incl - min_incl + 1) + min_incl;
}

BOOST_AUTO_TEST_CASE(decode_rate) {
  FastRandomContext random(true);
  constexpr size_t TX_CACHE_SIZE = 20000;
  constexpr size_t TRIALS = 1000;
  constexpr size_t MAX_BLOCK_COUNT = 1000;
  // Corresponds to receiver-sender mempool difference of 5%
  constexpr float SENDER_RECEIVER_RATIO = 0.025;

  std::vector<CTransactionRef> txs;
  for (size_t i = 0; i < TX_CACHE_SIZE; ++i) {
    txs.emplace_back(CreateTx(i));
  }

  size_t successes = 0;

  size_t graphene_total_size = 0;
  size_t compact_total_size = 0;

  size_t absolute_best_size = 0;

  for (size_t i = 0; i < TRIALS; ++i) {
    CBlock block;
    MempoolMock sender;
    MempoolMock receiver;
    size_t sender_count = RandRange(0, txs.size(), random);
    size_t receiver_count =
        RandRange(sender_count * (1.0f - SENDER_RECEIVER_RATIO),
                  sender_count * (1.0f + SENDER_RECEIVER_RATIO), random);
    receiver_count = std::min(receiver_count, txs.size());
    size_t block_count = RandRange(0, std::min(MAX_BLOCK_COUNT, sender_count), random);

    std::random_shuffle(txs.begin(), txs.end());

    sender.txs.insert(sender.txs.end(),
                      txs.begin(),
                      txs.begin() + sender_count);
    receiver.txs.insert(receiver.txs.end(),
                        txs.begin(),
                        txs.begin() + receiver_count);

    block.vtx.insert(block.vtx.begin(), CreateCoinbase());
    block.vtx.insert(block.vtx.end(),
                     sender.txs.begin() + sender_count - block_count,
                     sender.txs.begin() + sender_count);

    const auto maybe_graphene = p2p::CreateGrapheneBlock(block,
                                                         sender.GetTxCount() - (block_count - 1),
                                                         receiver.GetTxCount(),
                                                         random);

    CBlockHeaderAndShortTxIDs cmpct_block(block);
    const size_t cmpct_size = GetSerializeSize(cmpct_block, PROTOCOL_VERSION);
    size_t graphene_size = 0;

    if (maybe_graphene) {
      const p2p::GrapheneBlock graphene = maybe_graphene.get();
      p2p::GrapheneBlockReconstructor r(graphene, receiver);
      graphene_size = GetSerializeSize(graphene, PROTOCOL_VERSION);

      if (r.GetState() != +p2p::GrapheneDecodeState::CANT_DECODE_IBLT) {
        ++successes;
      } else {
        // If graphene failed for some reason - we will have to send compact
        graphene_size += cmpct_size;
      }
    } else {
      // If graphene failed for some reason - we will have to send compact
      graphene_size = cmpct_size;
    }

    graphene_total_size += graphene_size;
    compact_total_size += cmpct_size;
    absolute_best_size += std::min(cmpct_size, graphene_size);
  }

  BOOST_CHECK(static_cast<double>(successes) / TRIALS > 0.95);
  BOOST_CHECK(absolute_best_size < compact_total_size * 3 / 4);
}

BOOST_AUTO_TEST_SUITE_END()
