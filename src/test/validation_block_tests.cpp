// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <injector.h>
#include <miner.h>
#include <random.h>
#include <test/test_unite.h>
#include <validation.h>
#include <validationinterface.h>
#include <snapshot/messages.h>
#include <esperanza/finalizationstate.h>
#include <wallet/test/wallet_test_fixture.h>

namespace {
UnitEInjectorConfiguration MakeConfig() {
  UnitEInjectorConfiguration config;
  config.disable_finalization = true;
  return config;
}
}

class TestChain100NoFinalizationSetup : public TestChain100Setup {
public:
  TestChain100NoFinalizationSetup() : TestChain100Setup(MakeConfig()) {}
};

BOOST_FIXTURE_TEST_SUITE(validation_block_tests, TestChain100NoFinalizationSetup)

struct TestSubscriber : public CValidationInterface {
    uint256 m_expected_tip;

    explicit TestSubscriber(uint256 tip) : m_expected_tip(tip) {}

    void UpdatedBlockTip(const CBlockIndex* pindexNew, const CBlockIndex* pindexFork, bool fInitialDownload) override
    {
        BOOST_CHECK_EQUAL(m_expected_tip, pindexNew->GetBlockHash());
    }

    void BlockConnected(const std::shared_ptr<const CBlock>& block, const CBlockIndex* pindex, const std::vector<CTransactionRef>& txnConflicted) override
    {
        BOOST_CHECK_EQUAL(m_expected_tip, block->hashPrevBlock);
        BOOST_CHECK_EQUAL(m_expected_tip, pindex->pprev->GetBlockHash());

        m_expected_tip = block->GetHash();
    }

    void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override
    {
        BOOST_CHECK_EQUAL(m_expected_tip, block->GetHash());

        m_expected_tip = block->hashPrevBlock;
    }
};

struct BlockData {
  std::shared_ptr<CBlock> block;
  CBlockIndex block_index;
  snapshot::SnapshotHash hash;
  uint32_t height;
};

BlockData Block(const BlockData &prevData)
{
    static int i = 0;
    static uint64_t time = Params().GenesisBlock().nTime;

    CScript pubKey;
    pubKey << i++ << OP_TRUE;

    auto ptemplate = BlockAssembler(Params()).CreateNewBlock(pubKey);
    auto pblock = std::make_shared<CBlock>(ptemplate->block);
    pblock->hashPrevBlock = prevData.block->GetHash();
    pblock->nTime = ++time;

    CMutableTransaction txCoinbase(*pblock->vtx[0]);
    txCoinbase.vout.resize(1);
    std::vector<uint8_t> snapshotHash = prevData.hash.GetHashVector(prevData.block_index);
    txCoinbase.vin[0].scriptSig = CScript() << (prevData.height + 1) << snapshotHash << OP_0;
    txCoinbase.vin[0].scriptWitness.SetNull();
    txCoinbase.vin[1].scriptWitness.SetNull();
    pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));

    snapshot::SnapshotHash newHash(prevData.hash.GetData());
    const COutPoint out(pblock->vtx[0]->GetHash(), 0);
    const Coin coin(pblock->vtx[0]->vout[0], prevData.height + 1, TxType::COINBASE);
    newHash.AddUTXO(snapshot::UTXO(out, coin));

    CBlockIndex bi;
    bi.stake_modifier = prevData.block_index.stake_modifier;
    bi.nBits = pblock->nBits;
    bi.nChainWork = prevData.block_index.nChainWork + GetBlockProof(bi);
    return BlockData{pblock, bi, newHash, prevData.height + 1};
}

std::shared_ptr<CBlock> FinalizeBlock(std::shared_ptr<CBlock> pblock)
{
    pblock->ComputeMerkleTrees();

    return pblock;
}

// construct a valid block
const BlockData GoodBlock(const BlockData& prevData)
{
    BlockData data = Block(prevData);
    FinalizeBlock(data.block);
    return data;
}

// construct an invalid block (but with a valid header)
const BlockData BadBlock(const BlockData& prevData)
{
    BlockData data = Block(prevData);

    CMutableTransaction coinbase_spend;
    coinbase_spend.vin.push_back(CTxIn(COutPoint(data.block->vtx[0]->GetHash(), 0), CScript(), 0));
    coinbase_spend.vout.push_back(data.block->vtx[0]->vout[0]);

    CTransactionRef tx = MakeTransactionRef(coinbase_spend);
    data.block->vtx.push_back(tx);

    FinalizeBlock(data.block);
    return data;
}

void BuildChain(const BlockData &root, int height, const unsigned int invalid_rate, const unsigned int branch_rate, const unsigned int max_size, std::vector<std::shared_ptr<const CBlock>>& blocks)
{
    if (height <= 0 || blocks.size() >= max_size) return;

    bool gen_invalid = InsecureRandRange(100) < invalid_rate;
    bool gen_fork = InsecureRandRange(100) < branch_rate;

    const BlockData blockData = gen_invalid ? BadBlock(root) : GoodBlock(root);
    blocks.push_back(blockData.block);
    if (!gen_invalid) {
        BuildChain(blockData, height - 1, invalid_rate, branch_rate, max_size, blocks);
    }

    if (gen_fork) {
        const BlockData data = GoodBlock(root);
        blocks.push_back(data.block);
        BuildChain(data, height - 1, invalid_rate, branch_rate, max_size, blocks);
    }
}

BOOST_AUTO_TEST_CASE(processnewblock_signals_ordering)
{
    // build a large-ish chain that's likely to have some forks
    std::vector<std::shared_ptr<const CBlock>> blocks;

    BlockData genesisData;
    {
        genesisData.block = std::make_shared<CBlock>(Params().GenesisBlock());
        genesisData.height = 0;

        CBlockIndex bi;
        bi.nBits = genesisData.block->nBits;
        bi.nChainWork = GetBlockProof(bi);
        genesisData.block_index = bi;

        for (size_t txIdx = 0; txIdx < genesisData.block->vtx.size(); ++txIdx) {
            auto &tx = genesisData.block->vtx[txIdx];
            for (size_t i = 0; i < tx->vout.size(); ++i) {
                auto &out = tx->vout[i];
                if (out.scriptPubKey.IsUnspendable()) {
                  continue;
                }

                const COutPoint outPoint(tx->GetHash(), i);
                const Coin coin(out, 0, txIdx == 0 ? TxType::COINBASE : TxType::REGULAR);
                genesisData.hash.AddUTXO(snapshot::UTXO(outPoint, coin));
            }
        }
    }

    while (blocks.size() < 50) {
        blocks.clear();
        BuildChain(genesisData, 100, 15, 10, 500, blocks);
    }

    bool ignored;
    CValidationState state;
    std::vector<CBlockHeader> headers;
    std::transform(blocks.begin(), blocks.end(), std::back_inserter(headers), [](std::shared_ptr<const CBlock> b) { return b->GetBlockHeader(); });

    // Process all the headers so we understand the toplogy of the chain
    BOOST_CHECK(ProcessNewBlockHeaders(headers, state, Params()));

    // Connect the genesis block and drain any outstanding events
    BOOST_CHECK(ProcessNewBlock(Params(), std::make_shared<CBlock>(Params().GenesisBlock()), true, &ignored));
    SyncWithValidationInterfaceQueue();

    // subscribe to events (this subscriber will validate event ordering)
    const CBlockIndex* initial_tip = nullptr;
    {
        LOCK(cs_main);
        initial_tip = chainActive.Tip();
    }
    TestSubscriber sub(initial_tip->GetBlockHash());
    RegisterValidationInterface(&sub);

    // create a bunch of threads that repeatedly process a block generated above at random
    // this will create parallelism and randomness inside validation - the ValidationInterface
    // will subscribe to events generated during block validation and assert on ordering invariance
    std::vector<std::thread> threads;
    // boost unit test is not thread safe as the checks record the results in some shared memory
    // which is not synchronized / does not happen under mutual exclusion.
    std::mutex cs;
    const auto check = [&cs](bool condition) {
      std::lock_guard<decltype(cs)> lock(cs);
      BOOST_CHECK(condition);
    };
    for (int i = 0; i < 10; i++) {
        threads.emplace_back([&]() {
            bool ignored;
            FastRandomContext insecure;
            for (int i = 0; i < 1000; i++) {
                auto block = blocks[insecure.randrange(blocks.size() - 1)];
                bool processed = ProcessNewBlock(Params(), block, true, &ignored);
                check(processed);
            }

            // to make sure that eventually we process the full chain - do it here
            for (auto block : blocks) {
                if (block->vtx.size() == 1) {
                    bool processed = ProcessNewBlock(Params(), block, true, &ignored);
                    check(processed);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }
    while (GetMainSignals().CallbacksPending() > 0) {
        MilliSleep(100);
    }

    UnregisterValidationInterface(&sub);

    BOOST_CHECK_EQUAL(sub.m_expected_tip, chainActive.Tip()->GetBlockHash());
}

BOOST_AUTO_TEST_SUITE_END()
