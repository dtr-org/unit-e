// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/validation.h>
#include <key.h>
#include <validation.h>
#include <miner.h>
#include <script/standard.h>
#include <script/sign.h>
#include <test/test_unite.h>
#include <utiltime.h>
#include <core_io.h>
#include <keystore.h>
#include <policy/policy.h>

#include <boost/test/unit_test.hpp>
#include <wallet/test/wallet_test_fixture.h>

bool CheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &inputs, bool fScriptChecks, unsigned int flags, bool cacheSigStore, bool cacheFullScriptStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck> *pvChecks);

BOOST_AUTO_TEST_SUITE(tx_validationcache_tests)

static bool
ToMemPool(const CMutableTransaction& tx)
{
    LOCK(cs_main);

    CValidationState state;
    return AcceptToMemoryPool(mempool, state, MakeTransactionRef(tx), nullptr /* pfMissingInputs */,
                              nullptr /* plTxnReplaced */, true /* bypass_limits */, 0 /* nAbsurdFee */);
}

BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend, TestChain100Setup)
{
    // Make sure skipping validation of transactions that were
    // validated going into the memory pool does not allow
    // double-spends in blocks to pass validation when they should not.

    CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID()));

    // Make a coinbase mature so we have something to spend
    const CTransactionRef last_coinbase = CreateAndProcessBlock({}, scriptPubKey).vtx[0];

    // Create a double-spend of mature coinbase txn:
    std::vector<CMutableTransaction> spends;
    spends.resize(2);
    for (int i = 0; i < 2; i++)
    {
        LOCK(m_wallet->cs_wallet);
        spends[i].nVersion = 1;
        spends[i].vin.resize(1);
        spends[i].vin[0].prevout.hash = last_coinbase->GetHash();
        spends[i].vin[0].prevout.n = 1;
        spends[i].vout.resize(1);
        spends[i].vout[0].nValue = 11*EEES;
        spends[i].vout[0].scriptPubKey = scriptPubKey;

        // Sign:
        m_wallet->SignTransaction(spends[i]);
    }

    CBlock block;
    bool processed;

    // Test 1: block with both of those transactions should be rejected.
    block = CreateAndProcessBlock(spends, scriptPubKey, boost::none, &processed);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());

    // Test 2: ... and should be rejected if spend1 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[0]));
    block = CreateAndProcessBlock(spends, scriptPubKey, boost::none, &processed);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
    mempool.clear();

    // Test 3: ... and should be rejected if spend2 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(spends, scriptPubKey, boost::none, &processed);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() != block.GetHash());
    mempool.clear();

    // Final sanity test: first spend in mempool, second in block, that's OK:
    std::vector<CMutableTransaction> oneSpend;
    oneSpend.push_back(spends[0]);
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(oneSpend, scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
    // spends[1] should have been removed from the mempool when the
    // block with spends[0] is accepted:
    BOOST_CHECK_EQUAL(mempool.size(), 0U);
}

// Run CheckInputs (using pcoinsTip) on the given transaction, for all script
// flags.  Test that CheckInputs passes for all flags that don't overlap with
// the failing_flags argument, but otherwise fails.
// CHECKLOCKTIMEVERIFY and CHECKSEQUENCEVERIFY (and future NOP codes that may
// get reassigned) have an interaction with DISCOURAGE_UPGRADABLE_NOPS: if
// the script flags used contain DISCOURAGE_UPGRADABLE_NOPS but don't contain
// CHECKLOCKTIMEVERIFY (or CHECKSEQUENCEVERIFY), but the script does contain
// OP_CHECKLOCKTIMEVERIFY (or OP_CHECKSEQUENCEVERIFY), then script execution
// should fail.
// Capture this interaction with the upgraded_nop argument: set it when evaluating
// any script flag that is implemented as an upgraded NOP code.
static void ValidateCheckInputsForAllFlags(const CTransaction &tx, uint32_t failing_flags, bool add_to_cache)
{
    PrecomputedTransactionData txdata(tx);
    // If we add many more flags, this loop can get too expensive, but we can
    // rewrite in the future to randomly pick a set of flags to evaluate.
    for (uint32_t test_flags=0; test_flags < (1U << 16); test_flags += 1) {
        CValidationState state;
        // Filter out incompatible flag choices
        if ((test_flags & SCRIPT_VERIFY_CLEANSTACK)) {
            // CLEANSTACK requires P2SH and WITNESS, see VerifyScript() in
            // script/interpreter.cpp
            test_flags |= SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        }
        if ((test_flags & SCRIPT_VERIFY_WITNESS)) {
            // WITNESS requires P2SH
            test_flags |= SCRIPT_VERIFY_P2SH;
        }
        bool ret = CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, nullptr);
        // CheckInputs should succeed iff test_flags doesn't intersect with
        // failing_flags
        bool expected_return_value = !(test_flags & failing_flags);
        BOOST_CHECK_EQUAL(ret, expected_return_value);

        // Test the caching
        if (ret && add_to_cache) {
            // Check that we get a cache hit if the tx was valid
            std::vector<CScriptCheck> scriptchecks;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, &scriptchecks));
            BOOST_CHECK(scriptchecks.empty());
        } else {
            // Check that we get script executions to check, if the transaction
            // was invalid, or we didn't add to cache.
            std::vector<CScriptCheck> scriptchecks;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, test_flags, true, add_to_cache, txdata, &scriptchecks));
            BOOST_CHECK_EQUAL(scriptchecks.size(), tx.vin.size());
        }
    }
}

BOOST_FIXTURE_TEST_CASE(checkinputs_test, TestChain100Setup)
{
    // Test that passing CheckInputs with one set of script flags doesn't imply
    // that we would pass again with a different set of flags.
    {
        LOCK(cs_main);
        InitScriptExecutionCache();
    }

    CScript p2pkh_scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    CScript p2sh_scriptPubKey = GetScriptForDestination(CScriptID(p2pkh_scriptPubKey));
    CScript p2wpkh_scriptPubKey = GetScriptForWitness(p2pkh_scriptPubKey);

    bool processed;
    const CTransactionRef p2pkh_coinbase = CreateAndProcessBlock({}, p2pkh_scriptPubKey, boost::none, &processed).vtx[0];
    BOOST_CHECK(processed);

    // Make the reward with p2pkh_scriptPubKey mature
    for (int i = 0; i < COINBASE_MATURITY + 1; ++i) {
      CreateAndProcessBlock({}, GetScriptForDestination(WitnessV0KeyHash(coinbaseKey.GetPubKey().GetID())));
    }

    m_wallet->AddCScript(p2pkh_scriptPubKey);

    // flags to test: SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, SCRIPT_VERIFY_CHECKSEQUENCE_VERIFY, SCRIPT_VERIFY_NULLDUMMY, uncompressed pubkey thing

    // Create 2 outputs that match the three scripts above, spending the first
    // coinbase tx.
    CMutableTransaction dersig_invalid_tx;

    dersig_invalid_tx.nVersion = 1;
    dersig_invalid_tx.vin.resize(1);
    dersig_invalid_tx.vin[0].prevout.hash = p2pkh_coinbase->GetHash();
    dersig_invalid_tx.vin[0].prevout.n = 0;
    dersig_invalid_tx.vout.resize(4);
    dersig_invalid_tx.vout[0].nValue = 11*EEES;
    dersig_invalid_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
    dersig_invalid_tx.vout[1].nValue = 11*EEES;
    dersig_invalid_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
    dersig_invalid_tx.vout[2].nValue = 11*EEES;
    dersig_invalid_tx.vout[2].scriptPubKey = CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    dersig_invalid_tx.vout[3].nValue = 11*EEES;
    dersig_invalid_tx.vout[3].scriptPubKey = CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

    // Sign, with a non-DER signature
    {
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(p2pkh_scriptPubKey, dersig_invalid_tx, 0, SIGHASH_ALL, m_coinbase_txns.back().vout[0].nValue, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char) 0); // padding byte makes this non-DER
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        dersig_invalid_tx.vin[0].scriptSig = CScript() << vchSig << ToByteVector(coinbaseKey.GetPubKey());
    }

    // Test the invalidity of a transaction not signed using strict DER
    {
        LOCK(cs_main);

        CValidationState state;
        PrecomputedTransactionData ptd_spend_tx(dersig_invalid_tx);

        BOOST_CHECK(!CheckInputs(dersig_invalid_tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, nullptr));

        // If we call again asking for scriptchecks (as happens in
        // ConnectBlock), we should add a script check object for this -- we're
        // not caching invalidity (if that changes, delete this test case).
        std::vector<CScriptCheck> scriptchecks;
        BOOST_CHECK(CheckInputs(dersig_invalid_tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG, true, true, ptd_spend_tx, &scriptchecks));
        BOOST_CHECK_EQUAL(scriptchecks.size(), 1U);

        // Check that the invalid transaction is in fact recognized as invalid
        // under the strict DER flags.
        ValidateCheckInputsForAllFlags(dersig_invalid_tx, SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_STRICTENC, false);
    }

    CMutableTransaction spend_tx;

    spend_tx.nVersion = 1;
    spend_tx.vin.resize(1);
    spend_tx.vin[0].prevout.hash = p2pkh_coinbase->GetHash();
    spend_tx.vin[0].prevout.n = 0;
    spend_tx.vout.resize(4);
    spend_tx.vout[0].nValue = 11*EEES;
    spend_tx.vout[0].scriptPubKey = p2sh_scriptPubKey;
    spend_tx.vout[1].nValue = 11*EEES;
    spend_tx.vout[1].scriptPubKey = p2wpkh_scriptPubKey;
    spend_tx.vout[2].nValue = 11*EEES;
    spend_tx.vout[2].scriptPubKey = CScript() << OP_CHECKLOCKTIMEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;
    spend_tx.vout[3].nValue = 11*EEES;
    spend_tx.vout[3].scriptPubKey = CScript() << OP_CHECKSEQUENCEVERIFY << OP_DROP << ToByteVector(coinbaseKey.GetPubKey()) << OP_CHECKSIG;

    // Sign, with a DER signature
    {
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(p2pkh_scriptPubKey, dersig_invalid_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        spend_tx.vin[0].scriptSig = CScript() << vchSig << ToByteVector(coinbaseKey.GetPubKey());
    }

    // And if we produce a block with this tx, it should be valid,
    // even though there's no cache entry.
    CBlock block;

    // Lock the coin so it cannot be used for staking
    {
        LOCK(m_wallet->cs_wallet);
        m_wallet->LockCoin(COutPoint(p2pkh_coinbase->GetHash(), 1));
    }

    block = CreateAndProcessBlock({spend_tx}, p2pkh_scriptPubKey);
    BOOST_CHECK(chainActive.Tip()->GetBlockHash() == block.GetHash());
    BOOST_CHECK(pcoinsTip->GetBestBlock() == block.GetHash());

    LOCK(cs_main);

    // Test P2SH: construct a transaction that is valid without P2SH, and
    // then test validity with P2SH.
    {
        CMutableTransaction invalid_under_p2sh_tx;
        invalid_under_p2sh_tx.nVersion = 1;
        invalid_under_p2sh_tx.vin.resize(1);
        invalid_under_p2sh_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_under_p2sh_tx.vin[0].prevout.n = 0;
        invalid_under_p2sh_tx.vout.resize(1);
        invalid_under_p2sh_tx.vout[0].nValue = 11*EEES;
        invalid_under_p2sh_tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;
        std::vector<unsigned char> vchSig2(p2pkh_scriptPubKey.begin(), p2pkh_scriptPubKey.end());
        invalid_under_p2sh_tx.vin[0].scriptSig << vchSig2;

        ValidateCheckInputsForAllFlags(invalid_under_p2sh_tx, SCRIPT_VERIFY_P2SH, true);
    }

    // Test CHECKLOCKTIMEVERIFY
    {
        CMutableTransaction invalid_with_cltv_tx;
        invalid_with_cltv_tx.nVersion = 1;
        invalid_with_cltv_tx.nLockTime = 100;
        invalid_with_cltv_tx.vin.resize(1);
        invalid_with_cltv_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_with_cltv_tx.vin[0].prevout.n = 2;
        invalid_with_cltv_tx.vin[0].nSequence = 0;
        invalid_with_cltv_tx.vout.resize(1);
        invalid_with_cltv_tx.vout[0].nValue = 11*EEES;
        invalid_with_cltv_tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;

        // Sign
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(spend_tx.vout[2].scriptPubKey, invalid_with_cltv_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

        ValidateCheckInputsForAllFlags(invalid_with_cltv_tx, SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true);

        // Make it valid, and check again
        invalid_with_cltv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
        CValidationState state;
        PrecomputedTransactionData txdata(invalid_with_cltv_tx);
        BOOST_CHECK(CheckInputs(invalid_with_cltv_tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY, true, true, txdata, nullptr));
    }

    // TEST CHECKSEQUENCEVERIFY
    {
        CMutableTransaction invalid_with_csv_tx;
        invalid_with_csv_tx.nVersion = 2;
        invalid_with_csv_tx.vin.resize(1);
        invalid_with_csv_tx.vin[0].prevout.hash = spend_tx.GetHash();
        invalid_with_csv_tx.vin[0].prevout.n = 3;
        invalid_with_csv_tx.vin[0].nSequence = 100;
        invalid_with_csv_tx.vout.resize(1);
        invalid_with_csv_tx.vout[0].nValue = 11*EEES;
        invalid_with_csv_tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;

        // Sign
        std::vector<unsigned char> vchSig;
        uint256 hash = SignatureHash(spend_tx.vout[3].scriptPubKey, invalid_with_csv_tx, 0, SIGHASH_ALL, 0, SigVersion::BASE);
        BOOST_CHECK(coinbaseKey.Sign(hash, vchSig));
        vchSig.push_back((unsigned char)SIGHASH_ALL);
        invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 101;

        ValidateCheckInputsForAllFlags(invalid_with_csv_tx, SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true);

        // Make it valid, and check again
        invalid_with_csv_tx.vin[0].scriptSig = CScript() << vchSig << 100;
        CValidationState state;
        PrecomputedTransactionData txdata(invalid_with_csv_tx);
        BOOST_CHECK(CheckInputs(invalid_with_csv_tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_CHECKSEQUENCEVERIFY, true, true, txdata, nullptr));
    }

    // TODO: add tests for remaining script flags

    // Test that passing CheckInputs with a valid witness doesn't imply success
    // for the same tx with a different witness.
    {
        CMutableTransaction valid_with_witness_tx;
        valid_with_witness_tx.nVersion = 1;
        valid_with_witness_tx.vin.resize(1);
        valid_with_witness_tx.vin[0].prevout.hash = spend_tx.GetHash();
        valid_with_witness_tx.vin[0].prevout.n = 1;
        valid_with_witness_tx.vout.resize(1);
        valid_with_witness_tx.vout[0].nValue = 11*EEES;
        valid_with_witness_tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;

        // Sign
        SignatureData sigdata;
        ProduceSignature(*m_wallet, MutableTransactionSignatureCreator(&valid_with_witness_tx, 0, 11*EEES, SIGHASH_ALL), spend_tx.vout[1].scriptPubKey, sigdata);
        UpdateInput(valid_with_witness_tx.vin[0], sigdata);

        // This should be valid under all script flags.
        ValidateCheckInputsForAllFlags(valid_with_witness_tx, 0, true);

        // Remove the witness, and check that it is now invalid.
        valid_with_witness_tx.vin[0].scriptWitness.SetNull();
        ValidateCheckInputsForAllFlags(valid_with_witness_tx, SCRIPT_VERIFY_WITNESS, true);
    }

    {
        // Test a transaction with multiple inputs.
        CMutableTransaction tx;

        tx.nVersion = 1;
        tx.vin.resize(2);
        tx.vin[0].prevout.hash = spend_tx.GetHash();
        tx.vin[0].prevout.n = 0;
        tx.vin[1].prevout.hash = spend_tx.GetHash();
        tx.vin[1].prevout.n = 1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 22*EEES;
        tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;

        // Sign
        for (int i=0; i<2; ++i) {
            SignatureData sigdata;
            ProduceSignature(*m_wallet, MutableTransactionSignatureCreator(&tx, i, 11*EEES, SIGHASH_ALL), spend_tx.vout[i].scriptPubKey, sigdata);
            UpdateInput(tx.vin[i], sigdata);
        }

        // This should be valid under all script flags
        ValidateCheckInputsForAllFlags(tx, 0, true);

        // Check that if the second input is invalid, but the first input is
        // valid, the transaction is not cached.
        // Invalidate vin[1]
        tx.vin[1].scriptWitness.SetNull();

        CValidationState state;
        PrecomputedTransactionData txdata(tx);
        // This transaction is now invalid under segwit, because of the second input.
        BOOST_CHECK(!CheckInputs(tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, nullptr));

        std::vector<CScriptCheck> scriptchecks;
        // Make sure this transaction was not cached (ie because the first
        // input was valid)
        BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, &scriptchecks));
        // Should get 2 script checks back -- caching is on a whole-transaction basis.
        BOOST_CHECK_EQUAL(scriptchecks.size(), 2U);
    }

    {
      // Test a coinbase transaction
      CMutableTransaction tx;

      tx.SetVersion(1);
      tx.SetType(TxType::COINBASE);
      tx.vin.resize(2);
      tx.vin[0] = CTxIn(uint256(), 0, CScript()); // meta input
      tx.vin[1].prevout.hash = spend_tx.GetHash();
      tx.vin[1].prevout.n = 0;
      tx.vout.resize(1);
      tx.vout[0].nValue = 22*EEES;
      tx.vout[0].scriptPubKey = p2pkh_scriptPubKey;

      {
        LOCK(m_wallet->GetWalletExtension().GetLock());
        m_wallet->GetWalletExtension().SignCoinbaseTransaction(tx);
      }

      CValidationState state;
      PrecomputedTransactionData txdata(tx);

      BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS, true, true, txdata, nullptr));
    }
}

BOOST_AUTO_TEST_SUITE_END()
