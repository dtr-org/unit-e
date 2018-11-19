// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <esperanza/vote.h>
#include <keystore.h>
#include <test/test_unite.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <util.h>

BOOST_FIXTURE_TEST_SUITE(interpreter_tests, ReducedTestingSetup)

uint256 GetPrevoutHash(const CTransaction& txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}

uint256 GetSequenceHash(const CTransaction& txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

uint256 GetOutputsHash(const CTransaction& txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto& txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetHash();
}

BOOST_AUTO_TEST_CASE(signaturehash_vote)
{
    SeedInsecureRand();
    CBasicKeyStore keystore;
    CKey k;
    InsecureNewKey(k, true);
    keystore.AddKey(k);

    CPubKey pk = k.GetPubKey();

    CScript prevScriptPK = CScript::CreatePayVoteSlashScript(pk);
    esperanza::Vote vote{pk.GetID(), GetRandHash(), 10, 100};

    CMutableTransaction tx;
    tx.SetType(TxType::VOTE);
    std::vector<unsigned char> voteSig;
    BOOST_CHECK(k.Sign(GetRandHash(), voteSig));
    CTxIn txin(GetRandHash(), 0, CScript::EncodeVote(vote, voteSig));
    tx.vin.push_back(txin);
    CAmount amount = 10000;

    CTxOut out;
    out.nValue = amount;
    out.scriptPubKey = prevScriptPK;

    tx.vout.push_back(out);

    CHashWriter ss(SER_GETHASH, 0);
    ss << tx.nVersion;
    ss << GetPrevoutHash(tx);
    ss << GetSequenceHash(tx);
    ss << tx.vin[0].prevout;
    ss << tx.vin[0].scriptSig;
    ss << amount;
    ss << tx.vin[0].nSequence;
    ss << GetOutputsHash(tx);
    ss << tx.nLockTime;
    ss << (int)SIGHASH_ALL;

    uint256 expectedHash = ss.GetHash();

    uint256 hash = SignatureHash(prevScriptPK, tx, 0, SIGHASH_ALL, amount, SIGVERSION_WITNESS_V0);
    BOOST_CHECK_EQUAL(hash, expectedHash);
}

BOOST_AUTO_TEST_SUITE_END()
