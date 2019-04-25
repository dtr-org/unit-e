// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>

#include <consensus/validation.h>
#include <rpc/server.h>
#include <test/test_unite.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/test/wallet_test_fixture.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>

extern UniValue importmulti(const JSONRPCRequest& request);
extern UniValue dumpwallet(const JSONRPCRequest& request);
extern UniValue importwallet(const JSONRPCRequest& request);

BOOST_FIXTURE_TEST_SUITE(wallet_tests, WalletTestingSetup)

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

BOOST_FIXTURE_TEST_CASE(rescan, TestChain100Setup)
{
    // Cap last block file size, and mine new block in a new block file.
    CBlockIndex* const nullBlock = nullptr;
    CBlockIndex* oldTip = chainActive.Tip();
    GetBlockFileInfo(oldTip->GetBlockPos().nFile)->nSize = MAX_BLOCKFILE_SIZE;
    CTransactionRef new_coinbase = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0];
    CBlockIndex* newTip = chainActive.Tip();

    RemoveWallet(m_wallet);

    LOCK(cs_main);

    // Verify ScanForWalletTransactions picks up transactions in both the old
    // and new block files.
    {
        CWallet wallet("dummy", WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        BOOST_CHECK_EQUAL(nullBlock, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
        const CAmount expected = m_coinbase_txns.back().GetValueOut() - m_coinbase_txns.back().vout.back().nValue
            + new_coinbase->GetValueOut() - new_coinbase->vout.back().nValue;
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), expected);
    }

    // Prune the older block file.
    PruneOneBlockFile(oldTip->GetBlockPos().nFile);
    UnlinkPrunedFiles({oldTip->GetBlockPos().nFile});

    // Verify ScanForWalletTransactions only picks transactions in the new block
    // file.
    {
        CWallet wallet("dummy", WalletDatabase::CreateDummy());
        AddKey(wallet, coinbaseKey);
        WalletRescanReserver reserver(&wallet);
        reserver.reserve();
        BOOST_CHECK_EQUAL(oldTip, wallet.ScanForWalletTransactions(oldTip, nullptr, reserver));
        BOOST_CHECK_EQUAL(wallet.GetImmatureBalance(), new_coinbase->GetValueOut() - new_coinbase->vout.back().nValue);
    }

    // Verify importmulti RPC returns failure for a key whose creation time is
    // before the missing block, and success for a key whose creation time is
    // after.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());
        AddWallet(wallet);
        UniValue keys;
        keys.setArray();
        UniValue key;
        key.setObject();
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(coinbaseKey.GetPubKey())));
        key.pushKV("timestamp", 0);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        key.clear();
        key.setObject();
        CKey futureKey;
        futureKey.MakeNewKey(true);
        key.pushKV("scriptPubKey", HexStr(GetScriptForRawPubKey(futureKey.GetPubKey())));
        key.pushKV("timestamp", newTip->GetBlockTimeMax() + TIMESTAMP_WINDOW + 1);
        key.pushKV("internal", UniValue(true));
        keys.push_back(key);
        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(keys);

        UniValue response = importmulti(request);
        BOOST_CHECK_EQUAL(response.write(),
            strprintf("[{\"success\":false,\"error\":{\"code\":-1,\"message\":\"Rescan failed for key with creation "
                      "timestamp %d. There was an error reading a block from time %d, which is after or within %d "
                      "seconds of key creation, and could contain transactions pertaining to the key. As a result, "
                      "transactions and coins using this key may not appear in the wallet. This error could be caused "
                      "by pruning or data corruption (see unit-e log for details) and could be dealt with by "
                      "downloading and rescanning the relevant blocks (see -reindex and -rescan "
                      "options).\"}},{\"success\":true}]",
                              0, oldTip->GetBlockTimeMax(), TIMESTAMP_WINDOW));
        RemoveWallet(wallet);
    }
}

// Verify importwallet RPC starts rescan at earliest block with timestamp
// greater or equal than key birthday. Previously there was a bug where
// importwallet RPC would start the scan at the latest block with timestamp less
// than or equal to key birthday.
BOOST_FIXTURE_TEST_CASE(importwallet_rescan, TestChain100Setup)
{
    // Create two blocks with same timestamp to verify that importwallet rescan
    // will pick up both blocks, not just the first.
    const int64_t BLOCK_TIME = chainActive.Tip()->GetBlockTimeMax() + 5;
    SetMockTime(BLOCK_TIME);
    m_coinbase_txns.emplace_back(*CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);
    m_coinbase_txns.emplace_back(*CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

    // Set key birthday to block time increased by the timestamp window, so
    // rescan will start at the block time.
    const int64_t KEY_TIME = BLOCK_TIME + TIMESTAMP_WINDOW;
    SetMockTime(KEY_TIME);
    m_coinbase_txns.emplace_back(*CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey())).vtx[0]);

    RemoveWallet(m_wallet);

    LOCK(cs_main);

    std::string backup_file = (SetDataDir("importwallet_rescan") / "wallet.backup").string();

    // Import key into wallet and call dumpwallet to create backup file.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());
        LOCK(wallet->cs_wallet);
        wallet->mapKeyMetadata[coinbaseKey.GetPubKey().GetID()].nCreateTime = KEY_TIME;
        wallet->AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::dumpwallet(request);
        RemoveWallet(wallet);
    }

    // Call importwallet RPC and verify all blocks with timestamps >= BLOCK_TIME
    // were scanned, and no prior blocks were scanned.
    {
        std::shared_ptr<CWallet> wallet = std::make_shared<CWallet>("dummy", WalletDatabase::CreateDummy());

        JSONRPCRequest request;
        request.params.setArray();
        request.params.push_back(backup_file);
        AddWallet(wallet);
        ::importwallet(request);
        RemoveWallet(wallet);

        LOCK(wallet->cs_wallet);
        BOOST_CHECK_EQUAL(wallet->mapWallet.size(), 3U);
        BOOST_CHECK_EQUAL(m_coinbase_txns.size(), 103U);
        for (size_t i = 0; i < m_coinbase_txns.size(); ++i) {
            bool found = wallet->GetWalletTx(m_coinbase_txns[i].GetHash());
            bool expected = i >= 100;
            BOOST_CHECK_EQUAL(found, expected);
        }
    }

    SetMockTime(0);
}

// Check that GetImmatureCredit() returns a newly calculated value instead of
// the cached value after a MarkDirty() call.
//
// This is a regression test written to verify a bugfix for the immature credit
// function. Similar tests probably should be written for the other credit and
// debit functions.
BOOST_FIXTURE_TEST_CASE(coin_mark_dirty_immature_credit, TestChain100Setup)
{
    CWallet wallet("dummy", WalletDatabase::CreateDummy());
    CWalletTx wtx(&wallet, MakeTransactionRef(m_coinbase_txns.back()));
    LOCK2(cs_main, wallet.cs_wallet);
    wtx.hashBlock = chainActive.Tip()->GetBlockHash();
    wtx.nIndex = 0;

    // Call GetImmatureCredit() once before adding the key to the wallet to
    // cache the current immature credit amount, which is 0.
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), 0);

    // Invalidate the cached value, add the key, and make sure a new immature
    // credit amount is calculated.
    wtx.MarkDirty();
    wallet.AddKeyPubKey(coinbaseKey, coinbaseKey.GetPubKey());
    BOOST_CHECK_EQUAL(wtx.GetImmatureCredit(), wtx.tx->vout[0].nValue);
}

BOOST_FIXTURE_TEST_CASE(get_immature_credit, TestChain100Setup)
{
  // Make the first coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  {
    LOCK(cs_main);
    const CWalletTx *const immature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.back().GetHash());
    BOOST_CHECK_EQUAL(immature_coinbase->GetImmatureCredit(), immature_coinbase->tx->vout[0].nValue);

    const CWalletTx *const mature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    BOOST_CHECK_EQUAL(mature_coinbase->GetImmatureCredit(), 0);
  }

  // Make the second coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

  {
    LOCK(cs_main);
    const CWalletTx *const immature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.at(2).GetHash());
    BOOST_CHECK_EQUAL(immature_coinbase->GetImmatureCredit(), immature_coinbase->tx->vout[0].nValue);

    const CWalletTx *const mature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.at(1).GetHash());
    BOOST_CHECK_EQUAL(mature_coinbase->GetImmatureCredit(), 0);
  }
}

BOOST_FIXTURE_TEST_CASE(get_available_credit, TestChain100Setup)
{
  // Make the first coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  {
    LOCK(cs_main);
    const CWalletTx *const immature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.back().GetHash());
    BOOST_CHECK_EQUAL(immature_coinbase->GetAvailableCredit(), 0);

    const CWalletTx *const mature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    BOOST_CHECK_EQUAL(mature_coinbase->GetAvailableCredit(), mature_coinbase->tx->vout[0].nValue);
  }

  // Make the second coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

  {
    LOCK(cs_main);
    const CWalletTx *const immature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.at(2).GetHash());
    BOOST_CHECK_EQUAL(immature_coinbase->GetAvailableCredit(), 0);

    const CWalletTx *const mature_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.at(1).GetHash());
    BOOST_CHECK_EQUAL(mature_coinbase->GetAvailableCredit(), immature_coinbase->tx->vout[0].nValue);
  }
}

BOOST_FIXTURE_TEST_CASE(get_immature_watch_only_credit, TestChain100Setup)
{
  CKey watch_only_key;
  watch_only_key.MakeNewKey(true);
  const CScript watch_only_script = GetScriptForRawPubKey(watch_only_key.GetPubKey());
  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddWatchOnly(watch_only_script, 0));
  }

  CTransactionRef immature_coinbase = CreateAndProcessBlock({}, watch_only_script).vtx[0];

  {
     LOCK(cs_main);
     const CWalletTx *const wallet_tx = m_wallet->GetWalletTx(immature_coinbase->GetHash());
     BOOST_CHECK_EQUAL(wallet_tx->GetImmatureWatchOnlyCredit(), immature_coinbase->vout[0].nValue);
  }

  // Make the coinbase watch-only mature
  for (int i = 0; i < COINBASE_MATURITY; ++i) {
    CreateAndProcessBlock({}, GetScriptForRawPubKey(watch_only_key.GetPubKey()));
  }

  {
    LOCK(cs_main);
    const CWalletTx *const wallet_tx = m_wallet->GetWalletTx(immature_coinbase->GetHash());
    BOOST_CHECK_EQUAL(wallet_tx->GetImmatureWatchOnlyCredit(), 0);
  }
}

BOOST_FIXTURE_TEST_CASE(get_available_watch_only_credit, TestChain100Setup)
{
  CKey watch_only_key;
  watch_only_key.MakeNewKey(true);
  const CScript watch_only_script = GetScriptForRawPubKey(watch_only_key.GetPubKey());
  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddWatchOnly(watch_only_script, 0));
  }

  CTransactionRef watch_only_coinbase = CreateAndProcessBlock({}, watch_only_script).vtx[0];

  {
    LOCK(cs_main);
    const CWalletTx *wallet_tx = m_wallet->GetWalletTx(watch_only_coinbase->GetHash());
    // The stake is not watch-only
    BOOST_CHECK_EQUAL(wallet_tx->GetAvailableCredit(false, ISMINE_WATCH_ONLY), 0 * UNIT);
  }

  // Make the coinbase watch-only mature mining using the rewards just made mature
  for (int i = 0; i < COINBASE_MATURITY; ++i) {
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  }

  {
    LOCK(cs_main);
    const CWalletTx *wallet_tx = m_wallet->GetWalletTx(watch_only_coinbase->GetHash());
    BOOST_CHECK_EQUAL(wallet_tx->GetAvailableCredit(false, ISMINE_WATCH_ONLY), watch_only_coinbase->vout[0].nValue);
  }
}

static int64_t AddTx(CWallet& wallet, uint32_t lockTime, int64_t mockTime, int64_t blockTime)
{
    CMutableTransaction tx;
    tx.nLockTime = lockTime;
    SetMockTime(mockTime);
    CBlockIndex* block = nullptr;
    if (blockTime > 0) {
        LOCK(cs_main);
        const auto inserted = mapBlockIndex.emplace(GetRandHash(), new CBlockIndex);
        assert(inserted.second);
        const uint256& hash = inserted.first->first;
        block = inserted.first->second;
        block->nTime = blockTime;
        block->phashBlock = &hash;
    }

    CWalletTx wtx(&wallet, MakeTransactionRef(tx));
    if (block) {
        wtx.SetMerkleBranch(block, 0);
    }
    {
        LOCK(cs_main);
        wallet.AddToWallet(wtx);
    }
    LOCK(wallet.cs_wallet);
    return wallet.mapWallet.at(wtx.GetHash()).nTimeSmart;
}

// Simple test to verify assignment of CWalletTx::nSmartTime value. Could be
// expanded to cover more corner cases of smart time logic.
BOOST_AUTO_TEST_CASE(ComputeTimeSmart)
{
    // New transaction should use clock time if lower than block time.
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 1, 100, 120), 100);

    // Test that updating existing transaction does not change smart time.
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 1, 200, 220), 100);

    // New transaction should use clock time if there's no block time.
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 2, 300, 0), 300);

    // New transaction should use block time if lower than clock time.
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 3, 420, 400), 400);

    // New transaction should use latest entry time if higher than
    // min(block time, clock time).
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 4, 500, 390), 400);

    // If there are future entries, new transaction should use time of the
    // newest entry that is no more than 300 seconds ahead of the clock time.
    BOOST_CHECK_EQUAL(AddTx(*m_wallet, 5, 50, 600), 300);

    // Reset mock time for other tests.
    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(LoadReceiveRequests)
{
    CTxDestination dest = CKeyID();
    LOCK(m_wallet->cs_wallet);
    m_wallet->AddDestData(dest, "misc", "val_misc");
    m_wallet->AddDestData(dest, "rr0", "val_rr0");
    m_wallet->AddDestData(dest, "rr1", "val_rr1");

    auto values = m_wallet->GetDestValues("rr");
    BOOST_CHECK_EQUAL(values.size(), 2U);
    BOOST_CHECK_EQUAL(values[0], "val_rr0");
    BOOST_CHECK_EQUAL(values[1], "val_rr1");
}

class ListCoinsTestingSetup : public TestChain100Setup
{
public:
    ListCoinsTestingSetup()
    {
        CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
        wallet = MakeUnique<CWallet>("mock", WalletDatabase::CreateMock());
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
    }

    ~ListCoinsTestingSetup()
    {
        wallet.reset();
    }

    CWalletTx& AddTx(CRecipient recipient)
    {
        CTransactionRef tx;
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        int changePos = -1;
        std::string error;
        CCoinControl dummy;
        BOOST_CHECK(wallet->CreateTransaction({recipient}, tx, reservekey, fee, changePos, error, dummy));
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(tx, {}, {}, {}, reservekey, nullptr, state));
        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(tx->GetHash()).tx);
        }
        CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
        LOCK(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(tx->GetHash());
        BOOST_CHECK(it != wallet->mapWallet.end());
        it->second.SetMerkleBranch(chainActive.Tip(), 1);
        return it->second;
    }

    std::unique_ptr<CWallet> wallet;
};

BOOST_FIXTURE_TEST_CASE(ListCoins, ListCoinsTestingSetup)
{
    std::string coinbaseAddress = coinbaseKey.GetPubKey().GetID().ToString();

    // Confirm ListCoins initially returns 2 coins grouped under coinbaseKey
    // address.
    std::map<CTxDestination, std::vector<COutput>> list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(list.begin()->first.which(), 4);
    BOOST_CHECK_EQUAL(boost::get<WitnessV0KeyHash>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 2U); // Mature reward + inital stake

    // Check initial balance from one mature coinbase transaction + the initial funds.
    BOOST_CHECK_EQUAL(10000 * UNIT + m_coinbase_txns.back().vout[0].nValue, wallet->GetAvailableBalance());

    // Make another block reward mature so we can spend it for a transaction
    CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));

    // Add a transaction creating a change address, and confirm ListCoins still
    // returns the coins associated with the change address underneath the
    // coinbaseKey pubkey, even though the change address has a different
    // pubkey.
    AddTx(CRecipient{GetScriptForDestination(WitnessV0KeyHash()), 1 * UNIT, false /* subtract fee */});
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(list.begin()->first.which(), 4);
    BOOST_CHECK_EQUAL(boost::get<WitnessV0KeyHash>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 4U); // stake + change + 2 mature rewards

    // Lock all coins. Confirm number of available coins drops to 0.
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> available;
        wallet->AvailableCoins(available);
        BOOST_CHECK_EQUAL(available.size(), 4U);
    }
    for (const auto& group : list) {
        for (const auto& coin : group.second) {
            LOCK(wallet->cs_wallet);
            wallet->LockCoin(COutPoint(coin.tx->GetHash(), coin.i));
        }
    }
    {
        LOCK2(cs_main, wallet->cs_wallet);
        std::vector<COutput> available;
        wallet->AvailableCoins(available);
        BOOST_CHECK_EQUAL(available.size(), 0U);
    }
    // Confirm ListCoins still returns same result as before, despite coins
    // being locked.
    list = wallet->ListCoins();
    BOOST_CHECK_EQUAL(list.size(), 1U);
    BOOST_CHECK_EQUAL(list.begin()->first.which(), 4);
    BOOST_CHECK_EQUAL(boost::get<WitnessV0KeyHash>(list.begin()->first).ToString(), coinbaseAddress);
    BOOST_CHECK_EQUAL(list.begin()->second.size(), 4U);
}

BOOST_FIXTURE_TEST_CASE(AvailableCoins_coinbase_maturity, TestChain100Setup)
{
    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        std::vector<COutput> stake_available;
        m_wallet->AvailableCoins(stake_available);
        BOOST_CHECK_EQUAL(stake_available.size(), 1);
        BOOST_CHECK_EQUAL(stake_available[0].tx->tx->vout[stake_available[0].i].nValue, 10000 * UNIT);
    }

    // Make one coinbase mature
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

    {
        LOCK2(cs_main, m_wallet->cs_wallet);

        std::vector<COutput> available;
        m_wallet->AvailableCoins(available);
        // Stake + block reward are now available
        BOOST_CHECK_EQUAL(available.size(), 2);
    }
}

// Test that AvailableCoins follows coin control settings for
// ignoring remotely staked coins.
BOOST_FIXTURE_TEST_CASE(AvailableCoins, ListCoinsTestingSetup) {
    std::vector<COutput> coins;

    CKey our_key;
    CKey our_second_key;
    our_key.MakeNewKey(/* compressed: */ true);
    our_second_key.MakeNewKey(/* compressed: */ true);
    CScript witness_script = GetScriptForMultisig(1, {our_key.GetPubKey(), our_second_key.GetPubKey()});
    {
        LOCK(wallet->cs_wallet);
        wallet->AddKey(our_key);
        wallet->AddKey(our_second_key);
        wallet->AddCScript(witness_script);
    }

    CKey their_key;
    their_key.MakeNewKey(true);

    {
        LOCK2(cs_main, wallet->cs_wallet);

        wallet->AvailableCoins(coins);
        // One coinbase has reached maturity + the stake
        BOOST_CHECK_EQUAL(2, coins.size());
    }

    AddTx(CRecipient{
            CScript::CreateRemoteStakingKeyhashScript(
                    ToByteVector(their_key.GetPubKey().GetID()),
                    ToByteVector(our_key.GetPubKey().GetSha256())),
            1 * UNIT, false
    });

    AddTx(CRecipient{
            CScript::CreateRemoteStakingScripthashScript(
                    ToByteVector(their_key.GetPubKey().GetID()),
                    ToByteVector(Sha256(witness_script.begin(), witness_script.end()))),
            1 * UNIT, false
    });

    {
        LOCK2(cs_main, wallet->cs_wallet);

        wallet->AvailableCoins(coins);
        // Two coinbase and one remote staking output and the initial stake

        CCoinControl coin_control;
        coin_control.m_ignore_remote_staked = true;

        wallet->AvailableCoins(coins, true, &coin_control);
        // Remote staking output should be ignored
        BOOST_CHECK_EQUAL(4, coins.size());
    }
}

BOOST_FIXTURE_TEST_CASE(GetAddressBalances_coinbase_maturity, TestChain100Setup) {

  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const std::map<CTxDestination, CAmount> balances = m_wallet->GetAddressBalances();
    BOOST_CHECK_EQUAL(balances.size(), 1); // the stake
  }

  // Make one coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

  {
    const CTxDestination coinbase_destination = GetDestinationForKey(coinbaseKey.GetPubKey(), OutputType::BECH32);
    LOCK2(cs_main, m_wallet->cs_wallet);
    const std::map<CTxDestination, CAmount> balances = m_wallet->GetAddressBalances();
    BOOST_CHECK_EQUAL(balances.size(), 1); // the stake and the reward with the same destination
    BOOST_CHECK_EQUAL(balances.at(coinbase_destination), m_coinbase_txns.front().GetValueOut());
  }
}

BOOST_FIXTURE_TEST_CASE(GetLegacyBalance_coinbase_maturity, TestChain100Setup) {

  // Nothing is mature currenly so no balances (except the inital stake)
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CAmount all_balance = m_wallet->GetLegacyBalance(ISMINE_ALL, 0, nullptr);
    const CAmount spendable_balance = m_wallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, nullptr);
    const CAmount watchonly_balance = m_wallet->GetLegacyBalance(ISMINE_WATCH_ONLY, 0, nullptr);
    BOOST_CHECK_EQUAL(all_balance, 10000 * UNIT);
    BOOST_CHECK_EQUAL(spendable_balance, 10000 * UNIT);
    BOOST_CHECK_EQUAL(watchonly_balance, 0);
  }

  // Make one coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

  // Now we should have the same balance as before plus the newly mature coinbase
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CAmount all_balance = m_wallet->GetLegacyBalance(ISMINE_ALL, 0, nullptr);
    const CAmount spendable_balance = m_wallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, nullptr);
    const CAmount watchonly_balance = m_wallet->GetLegacyBalance(ISMINE_WATCH_ONLY, 0, nullptr);
    BOOST_CHECK_EQUAL(all_balance, (10000 * UNIT) + m_coinbase_txns.front().vout[0].nValue);
    BOOST_CHECK_EQUAL(spendable_balance, (10000 * UNIT) + m_coinbase_txns.back().vout[0].nValue);
    BOOST_CHECK_EQUAL(watchonly_balance, 0);
  }

  // Now add a new watch-only key, create a new coinbase and then make it mature
  CKey watch_only_key;
  watch_only_key.MakeNewKey(true);
  const CScript watch_only_script = GetScriptForRawPubKey(watch_only_key.GetPubKey());

  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddWatchOnly(watch_only_script, 0));
  }

  // Make one more coinbase mature so we can use it to mine after we spent our
  // last output for creating the watch-only block
  CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));

  auto watch_only_coinbase = CreateAndProcessBlock({}, GetScriptForRawPubKey(watch_only_key.GetPubKey())).vtx[0];

  for (int i = 0; i < COINBASE_MATURITY + 1; ++i) {
    CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
  }

  // As per mature outputs we should have 103 blocks worth of rewards + the initial
  // stake + the watch-only reward + 100 finalization rewards
  {
      auto coinbase_reward = m_coinbase_txns.back().vout[0].nValue;
      auto finalization_reward = m_coinbase_txns[50].vout[1].nValue;
      LOCK2(cs_main, m_wallet->cs_wallet);
      const CAmount all_balance = m_wallet->GetLegacyBalance(ISMINE_ALL, 0, nullptr);
      const CAmount spendable_balance = m_wallet->GetLegacyBalance(ISMINE_SPENDABLE, 0, nullptr);
      const CAmount watchonly_balance = m_wallet->GetLegacyBalance(ISMINE_WATCH_ONLY, 0, nullptr);

      BOOST_CHECK_EQUAL(
          all_balance,
          (10000 * UNIT) + coinbase_reward * 103 + finalization_reward * 100 + watch_only_coinbase->vout[0].nValue
      );
      BOOST_CHECK_EQUAL(spendable_balance, (10000 * UNIT) + coinbase_reward * 103 + finalization_reward * 100);
      BOOST_CHECK_EQUAL(watchonly_balance, watch_only_coinbase->vout[0].nValue);
  }
}

BOOST_FIXTURE_TEST_CASE(GetBlockToMaturity, TestChain100Setup)
{
  // Make the first coinbase mature
  CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));

  const blockchain::Height height = chainActive.Height();
  {
    LOCK(cs_main);
    const CWalletTx *const first = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    BOOST_CHECK(first);
    // Height is 101, COINBASE_MATURITY is 100, so we expect the coinbase to be mature
    BOOST_CHECK_EQUAL(first->GetBlocksToRewardMaturity(), 0);

    const CWalletTx *const next_to_first = m_wallet->GetWalletTx(m_coinbase_txns.at(1).GetHash());
    BOOST_CHECK(next_to_first);
    BOOST_CHECK_EQUAL(next_to_first->GetBlocksToRewardMaturity(), 1);

    const CWalletTx *const middle = m_wallet->GetWalletTx(m_coinbase_txns.at(m_coinbase_txns.size()/2).GetHash());
    BOOST_CHECK(middle);
    BOOST_CHECK_EQUAL(middle->GetBlocksToRewardMaturity(), COINBASE_MATURITY - (height/2));

    // Just another block has been created on top of the last coibase, so we expect
    // it to need other COINBASE_MATURITY - 1 confirmations
    const CWalletTx *const last = m_wallet->GetWalletTx(m_coinbase_txns.back().GetHash());
    BOOST_CHECK(last);
    BOOST_CHECK_EQUAL(last->GetBlocksToRewardMaturity(), COINBASE_MATURITY - 1);
  }

  // Create 10 more blocks
  CBlock last_block;
  for (int i = 0; i < 10; ++i) {
    last_block = CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  }

  {
    LOCK(cs_main);
    CWalletTx last_generated_coinbase(m_wallet.get(), last_block.vtx[0]);
    BOOST_CHECK_EQUAL(last_generated_coinbase.GetBlocksToRewardMaturity(), COINBASE_MATURITY + 1);

    const CWalletTx *const last_coinbase = m_wallet->GetWalletTx(m_coinbase_txns.back().GetHash());
    BOOST_CHECK(last_coinbase);
    BOOST_CHECK_EQUAL(last_coinbase->GetBlocksToRewardMaturity(), COINBASE_MATURITY - 11);
  }
}

BOOST_FIXTURE_TEST_CASE(GetCredit_coinbase_maturity, TestChain100Setup) {

  // Nothing is mature currenly so no balances (except the initial stake)
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CWalletTx *const first = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    const CAmount all_credit = first->GetCredit(ISMINE_ALL);
    const CAmount spendable_credit = first->GetCredit(ISMINE_SPENDABLE);
    const CAmount watchonly_credit = first->GetCredit(ISMINE_WATCH_ONLY);
    BOOST_CHECK_EQUAL(all_credit, 10000 * UNIT);
    BOOST_CHECK_EQUAL(spendable_credit, 10000 * UNIT);
    BOOST_CHECK_EQUAL(watchonly_credit, 0);
  }

  // Make one coinbase mature
  CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));

  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CWalletTx *const first = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    const CAmount all_credit = first->GetCredit(ISMINE_ALL);
    const CAmount spendable_credit = first->GetCredit(ISMINE_SPENDABLE);
    const CAmount watchonly_credit = first->GetCredit(ISMINE_WATCH_ONLY);
    BOOST_CHECK_EQUAL(all_credit, m_coinbase_txns.back().GetValueOut());
    BOOST_CHECK_EQUAL(spendable_credit, m_coinbase_txns.back().GetValueOut());
    BOOST_CHECK_EQUAL(watchonly_credit, 0);
  }

  // Now add a new watch-only key, create a new coinbase and then make it mature
  CKey watch_only_key;
  watch_only_key.MakeNewKey(true);
  const CScript watch_only_script = GetScriptForRawPubKey(watch_only_key.GetPubKey());

  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddWatchOnly(watch_only_script, 0));
  }

  // Make one more coinbase mature so we can use it to mine after we spent our
  // last output for creating the watch-only block
  CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));

  CTransactionRef watch_only_coinbase = CreateAndProcessBlock({}, watch_only_script).vtx[0];

  for (int i = 0; i < COINBASE_MATURITY; ++i) {
    CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
  }

  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CWalletTx *const watch_only = m_wallet->GetWalletTx(watch_only_coinbase->GetHash());
    const CAmount all_credit = watch_only->GetCredit(ISMINE_ALL);
    const CAmount spendable_credit = watch_only->GetCredit(ISMINE_SPENDABLE);
    const CAmount watchonly_credit = watch_only->GetCredit(ISMINE_WATCH_ONLY);
    BOOST_CHECK_EQUAL(all_credit, watch_only_coinbase->GetValueOut());
    BOOST_CHECK_EQUAL(watchonly_credit, watch_only_coinbase->vout[0].nValue);
    BOOST_CHECK_EQUAL(spendable_credit, watch_only_coinbase->vout[1].nValue);
  }
}

BOOST_FIXTURE_TEST_CASE(GetCredit_coinbase_cache, TestChain100Setup) {
  // Nothing is mature (except the initial stake) currenlty so nothing should be cached
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CWalletTx *const first = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());
    const CAmount available_credit = first->GetAvailableCredit(true);
    const CAmount all_credit = first->GetCredit(ISMINE_ALL);
    BOOST_CHECK_EQUAL(all_credit, 10000 * UNIT);
    BOOST_CHECK_EQUAL(first->fCreditCached, false);
    BOOST_CHECK_EQUAL(first->nCreditCached, 0);
    BOOST_CHECK_EQUAL(first->fAvailableCreditCached, false);
    BOOST_CHECK_EQUAL(first->nAvailableCreditCached, 0);
    BOOST_CHECK_EQUAL(available_credit, 0);
  }

  // Make one coinbase mature
  CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    const CWalletTx *const first = m_wallet->GetWalletTx(m_coinbase_txns.front().GetHash());

    // Since we didn't call GetBalance or GetAvailableCredit yet, nothing should be cached
    BOOST_CHECK_EQUAL(first->fCreditCached, false);
    BOOST_CHECK_EQUAL(first->nCreditCached, 0);
    BOOST_CHECK_EQUAL(first->fAvailableCreditCached, false);
    BOOST_CHECK_EQUAL(first->nAvailableCreditCached, 0);

    // The available credit is just the mature reward because the stake has been
    // already spent at this point
    const CAmount all_credit = first->GetCredit(ISMINE_ALL);
    const CAmount available_credit = first->GetAvailableCredit(true);
    BOOST_CHECK_EQUAL(all_credit, (10000 * UNIT) + m_coinbase_txns.front().vout[0].nValue);
    BOOST_CHECK_EQUAL(available_credit, m_coinbase_txns.front().vout[0].nValue);
    BOOST_CHECK_EQUAL(first->fCreditCached, true);
    BOOST_CHECK_EQUAL(first->nCreditCached, (10000 * UNIT) + m_coinbase_txns.front().vout[0].nValue);
    BOOST_CHECK_EQUAL(first->fAvailableCreditCached, true);
    BOOST_CHECK_EQUAL(first->nAvailableCreditCached, m_coinbase_txns.front().vout[0].nValue);

    // Calling the second time should result in the same (cached) values
    BOOST_CHECK_EQUAL(all_credit, first->GetCredit(ISMINE_ALL));
    BOOST_CHECK_EQUAL(available_credit, first->GetAvailableCredit(true));

    // Change the cached values to verify that they are the ones used
    first->nCreditCached = all_credit - 5 * UNIT;
    first->nAvailableCreditCached = available_credit - 7 * UNIT;
    BOOST_CHECK_EQUAL(all_credit - 5 * UNIT, first->GetCredit(ISMINE_ALL));
    BOOST_CHECK_EQUAL(available_credit - 7 * UNIT, first->GetAvailableCredit(true));

    // Verify that the amounts will be recalculated properly
    first->fCreditCached = false;
    first->fAvailableCreditCached = false;
    BOOST_CHECK_EQUAL(all_credit, first->GetCredit(ISMINE_ALL));
    BOOST_CHECK_EQUAL(available_credit, first->GetAvailableCredit(true));
  }

  // Now add a new watch-only key, create a new coinbase and then make it mature
  CKey watch_only_key;
  watch_only_key.MakeNewKey(true);
  const CScript watch_only_script = GetScriptForRawPubKey(watch_only_key.GetPubKey());

  {
    LOCK(m_wallet->cs_wallet);
    assert(m_wallet->AddWatchOnly(watch_only_script, 0));
  }

  // The initial stake is gonna be used to generate this block and it will become
  // watch-only
  CTransactionRef watch_only_coinbase = CreateAndProcessBlock({}, GetScriptForRawPubKey(watch_only_key.GetPubKey())).vtx[0];

  for (int i = 0; i < COINBASE_MATURITY + 1; ++i) {
    CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
  }

  {
    LOCK2(cs_main, m_wallet->cs_wallet);

    const CWalletTx *const watch_only = m_wallet->GetWalletTx(watch_only_coinbase->GetHash());

    BOOST_CHECK_EQUAL(watch_only->fWatchCreditCached, false);
    BOOST_CHECK_EQUAL(watch_only->nWatchCreditCached, 0);
    BOOST_CHECK_EQUAL(watch_only->fAvailableWatchCreditCached, false);
    BOOST_CHECK_EQUAL(watch_only->nAvailableWatchCreditCached, 0);

    const CAmount watch_only_credit = watch_only->GetCredit(ISMINE_WATCH_ONLY);
    const CAmount available_watch_only_credit = watch_only->GetAvailableCredit(true, ISMINE_WATCH_ONLY);

    BOOST_CHECK_EQUAL(watch_only_credit, watch_only_coinbase->vout[0].nValue);
    BOOST_CHECK_EQUAL(available_watch_only_credit, watch_only_coinbase->vout[0].nValue);
    BOOST_CHECK_EQUAL(watch_only->fWatchCreditCached, true);
    BOOST_CHECK_EQUAL(watch_only->nWatchCreditCached, watch_only_coinbase->vout[0].nValue);
    BOOST_CHECK_EQUAL(watch_only->fAvailableWatchCreditCached, true);
    BOOST_CHECK_EQUAL(watch_only->nAvailableWatchCreditCached, watch_only_coinbase->vout[0].nValue);

    // Calling the second time should result in the same (cached) values
    BOOST_CHECK_EQUAL(watch_only_credit, watch_only->GetCredit(ISMINE_WATCH_ONLY));
    BOOST_CHECK_EQUAL(available_watch_only_credit, watch_only->GetAvailableCredit(true, ISMINE_WATCH_ONLY));

    // Verify cache is used
    watch_only->nWatchCreditCached = watch_only_credit - 1 * UNIT;
    watch_only->nAvailableWatchCreditCached = available_watch_only_credit - 2 * UNIT;
    BOOST_CHECK_EQUAL(watch_only_credit - 1 * UNIT, watch_only->GetCredit(ISMINE_WATCH_ONLY));
    BOOST_CHECK_EQUAL(available_watch_only_credit - 2 * UNIT, watch_only->GetAvailableCredit(true, ISMINE_WATCH_ONLY));

    // Verify that the amounts will be recalculated properly
    watch_only->fWatchCreditCached = false;
    watch_only->fAvailableWatchCreditCached = false;
    BOOST_CHECK_EQUAL(watch_only_credit, watch_only->GetCredit(ISMINE_WATCH_ONLY));
    BOOST_CHECK_EQUAL(available_watch_only_credit, watch_only->GetAvailableCredit(true, ISMINE_WATCH_ONLY));
  }
}

BOOST_AUTO_TEST_SUITE_END()
