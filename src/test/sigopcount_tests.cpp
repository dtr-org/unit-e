// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <pubkey.h>
#include <key.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>
#include <test/test_unite.h>

#include <vector>

#include <boost/test/unit_test.hpp>

// Helpers:
static std::vector<unsigned char>
Serialize(const CScript& s)
{
    std::vector<unsigned char> sSerialized(s.begin(), s.end());
    return sSerialized;
}

BOOST_FIXTURE_TEST_SUITE(sigopcount_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(GetSigOpCount)
{
    // Test CScript::GetSigOpCount()
    CScript s1;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(false), 0U);
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 0U);

    uint160 dummy;
    s1 << OP_1 << ToByteVector(dummy) << ToByteVector(dummy) << OP_2 << OP_CHECKMULTISIG;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 2U);
    s1 << OP_IF << OP_CHECKSIG << OP_ENDIF;
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(true), 3U);
    BOOST_CHECK_EQUAL(s1.GetSigOpCount(false), 21U);

    CScript p2sh = GetScriptForDestination(CScriptID(s1));
    CScript scriptSig;
    scriptSig << OP_0 << Serialize(s1);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(scriptSig), 3U);

    std::vector<CPubKey> keys;
    for (int i = 0; i < 3; i++)
    {
        CKey k;
        k.MakeNewKey(true);
        keys.push_back(k.GetPubKey());
    }
    CScript s2 = GetScriptForMultisig(1, keys);
    BOOST_CHECK_EQUAL(s2.GetSigOpCount(true), 3U);
    BOOST_CHECK_EQUAL(s2.GetSigOpCount(false), 20U);

    p2sh = GetScriptForDestination(CScriptID(s2));
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(true), 0U);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(false), 0U);
    CScript scriptSig2;
    scriptSig2 << OP_1 << ToByteVector(dummy) << ToByteVector(dummy) << Serialize(s2);
    BOOST_CHECK_EQUAL(p2sh.GetSigOpCount(scriptSig2), 3U);
}

/**
 * Verifies script execution of the zeroth scriptPubKey of tx output and
 * zeroth scriptSig and witness of tx input (or first if input tx is coinbase tx).
 */
static ScriptError VerifyWithFlag(const CTransaction& output, const CMutableTransaction& input, int flags)
{
    ScriptError error;
    const CTransaction inputi(input);
    const std::size_t input_ix = inputi.IsCoinBase() ? 1 : 0;
    BOOST_REQUIRE(inputi.vin.size() > input_ix);
    const CScript &script_sig = inputi.vin[input_ix].scriptSig;
    const CScript &script_pubkey = output.vout[0].scriptPubKey;
    const CScriptWitness *script_witness = &inputi.vin[input_ix].scriptWitness;
    const TransactionSignatureChecker checker(&inputi, input_ix, output.vout[0].nValue);
    const bool ret = VerifyScript(script_sig, script_pubkey, script_witness, flags, checker, &error);
    BOOST_CHECK(ret == (error == SCRIPT_ERR_OK));

    return error;
}

/**
 * Builds a creationTx from scriptPubKey and a spendingTx from scriptSig
 * and witness such that spendingTx spends output zero of creationTx.
 * Also inserts creationTx's output into the coins view.
 */
static void BuildTxs(CMutableTransaction& spendingTx, CCoinsViewCache& coins, CMutableTransaction& creationTx, const CScript& scriptPubKey, const CScript& scriptSig, const CScriptWitness& witness)
{
    creationTx.SetVersion(1);
    creationTx.SetType(TxType::COINBASE);
    creationTx.vin.resize(1);
    creationTx.vin[0].prevout.SetNull();
    creationTx.vin[0].scriptSig = CScript();
    creationTx.vout.resize(1);
    creationTx.vout[0].nValue = 1;
    creationTx.vout[0].scriptPubKey = scriptPubKey;

    spendingTx.nVersion = 1;
    spendingTx.vin.resize(1);
    spendingTx.vin[0].prevout.hash = creationTx.GetHash();
    spendingTx.vin[0].prevout.n = 0;
    spendingTx.vin[0].scriptSig = scriptSig;
    spendingTx.vin[0].scriptWitness = witness;
    spendingTx.vout.resize(1);
    spendingTx.vout[0].nValue = 1;
    spendingTx.vout[0].scriptPubKey = CScript();

    AddCoins(coins, CTransaction(creationTx), 0);
}

BOOST_AUTO_TEST_CASE(GetTxSigOpCost)
{
    // Transaction creates outputs
    CMutableTransaction creationTx;
    // Transaction that spends outputs and whose
    // sig op cost is going to be tested
    CMutableTransaction spendingTx;

    // Create utxo set
    CCoinsView coinsDummy;
    CCoinsViewCache coins(&coinsDummy);
    // Create key
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    // Default flags
    int flags = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_P2SH;

    // Multisig script (legacy counting)
    {
        CScript scriptPubKey = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        // Do not use a valid signature to avoid using wallet operations.
        CScript scriptSig = CScript() << OP_0 << OP_0;

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, CScriptWitness());
        // Legacy counting only includes signature operations in scriptSigs and scriptPubKeys
        // of a transaction and does not take the actual executed sig operations into account.
        // spendingTx in itself does not contain a signature operation.
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 0);
        // creationTx contains two signature operations in its scriptPubKey, but legacy counting
        // is not accurate.
        creationTx.SetType(TxType::COINBASE);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(creationTx), coins, flags), MAX_PUBKEYS_PER_MULTISIG * WITNESS_SCALE_FACTOR);
        // Sanity check: script verification fails because of an invalid signature.
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_CHECKMULTISIGVERIFY);
    }

    // Multisig nested in P2SH
    {
        CScript redeemScript = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        CScript scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));
        CScript scriptSig = CScript() << OP_0 << OP_0 << ToByteVector(redeemScript);

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, CScriptWitness());
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 2 * WITNESS_SCALE_FACTOR);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_CHECKMULTISIGVERIFY);
    }

    // P2WPKH witness program
    {
        CScript p2pk = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
        CScript scriptPubKey = GetScriptForWitness(p2pk);
        CScript scriptSig = CScript();
        CScriptWitness scriptWitness;
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));


        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 1);
        // No signature operations if we don't verify the witness.
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags & ~SCRIPT_VERIFY_WITNESS), 0);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_EQUALVERIFY);

        // The sig op cost for witness version > 2 is zero.
        BOOST_CHECK_EQUAL(scriptPubKey[0], 0x00);
        scriptPubKey[0] = 0x53;
        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 0);
        scriptPubKey[0] = 0x00;
        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);

        // The witness of a coinbase transaction is not taken into account.
        spendingTx.SetType(TxType::COINBASE);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 0);
    }

    // P2WPKH nested in P2SH
    {
        CScript p2pk = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
        CScript scriptSig = GetScriptForWitness(p2pk);
        CScript scriptPubKey = GetScriptForDestination(CScriptID(scriptSig));
        scriptSig = CScript() << ToByteVector(scriptSig);
        CScriptWitness scriptWitness;
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 1);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_EQUALVERIFY);
    }

    // P2WSH witness program
    {
        CScript witnessScript = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        CScript scriptPubKey = GetScriptForWitness(witnessScript);
        CScript scriptSig = CScript();
        CScriptWitness scriptWitness;
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 2);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags & ~SCRIPT_VERIFY_WITNESS), 0);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_CHECKMULTISIGVERIFY);
    }

    // P2WSH nested in P2SH
    {
        CScript witnessScript = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        CScript redeemScript = GetScriptForWitness(witnessScript);
        CScript scriptPubKey = GetScriptForDestination(CScriptID(redeemScript));
        CScript scriptSig = CScript() << ToByteVector(redeemScript);
        CScriptWitness scriptWitness;
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(0));
        scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 2);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_CHECKMULTISIGVERIFY);
    }

    // Remote staking P2PKH witness program
    {
        CScript scriptPubKey = CScript() << OP_1 << ToByteVector(pubkey.GetID()) << ToByteVector(pubkey.GetSha256());
        CScript scriptSig = CScript();
        CScriptWitness scriptWitness;
        scriptWitness.stack.emplace_back(0);
        scriptWitness.stack.emplace_back(0);

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 1);

        // No signature operations if we don't verify the witness.
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags & ~SCRIPT_VERIFY_WITNESS), 0);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_EQUALVERIFY);

        // The number of signature operations for RSP2PKH does not depend on the type of the transaction
        spendingTx.SetType(TxType::COINBASE);
        // push the coinbase meta input
        spendingTx.vin.insert(spendingTx.vin.begin(), CTxIn());
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 1);
        // No signature operations if we don't verify the witness (coinbase version)
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags & ~SCRIPT_VERIFY_WITNESS), 0);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_EQUALVERIFY);
    }

    // Remote staking P2WSH witness program
    {
        CScript witnessScript = CScript() << 1 << ToByteVector(pubkey) << ToByteVector(pubkey) << 2 << OP_CHECKMULTISIGVERIFY;
        CScript scriptPubKey = CScript() << OP_2 << ToByteVector(pubkey.GetID()) << ToByteVector(Sha256(witnessScript.begin(), witnessScript.end()));
        CScript scriptSig = CScript();
        CScriptWitness scriptWitness;
        scriptWitness.stack.emplace_back(0);
        scriptWitness.stack.emplace_back(0);
        scriptWitness.stack.emplace_back(witnessScript.begin(), witnessScript.end());

        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        spendingTx.SetType(TxType::REGULAR);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 2);
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags & ~SCRIPT_VERIFY_WITNESS), 0);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_CHECKMULTISIGVERIFY);

        // The number of signature operations for RSP2SH in a coinbase transaction always equals one
        scriptWitness.stack.pop_back();
        BuildTxs(spendingTx, coins, creationTx, scriptPubKey, scriptSig, scriptWitness);
        // make spending tx a coinbase transaction
        spendingTx.SetType(TxType::COINBASE);
        // push the coinbase meta input
        spendingTx.vin.insert(spendingTx.vin.begin(), CTxIn());
        BOOST_CHECK_EQUAL(GetTransactionSigOpCost(CTransaction(spendingTx), coins, flags), 1);
        BOOST_CHECK_EQUAL(VerifyWithFlag(CTransaction(creationTx), spendingTx, flags), SCRIPT_ERR_EQUALVERIFY);
    }
}

BOOST_AUTO_TEST_SUITE_END()
