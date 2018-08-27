#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <test/test_bitcoin.h>
#include <util.h>
#include <script/interpreter.h>
#include <script/script.cpp>
#include <pos/esperanzavote.h>
#include <keystore.h>

BOOST_FIXTURE_TEST_SUITE(interpreter_tests, BasicTestingSetup)

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

    if (txTo.IsParticlVersion())
    {
        for (unsigned int n = 0; n < txTo.vpout.size(); n++)
            ss << *txTo.vpout[n];
    } else
    {
        for (const auto& txout : txTo.vout) {
            ss << txout;
        }
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
    VoteData vote{GetRandHash(), GetRandHash(), 10, 100};

    CMutableTransaction tx;
    tx.SetType(TXN_VOTE);
    CTxIn txin(GetRandHash(), 0, CScript::EncodeVoteData(vote));
    tx.vin.push_back(txin);
    CAmount amount = 10000;
    std::vector<uint8_t> vchAmount(8);
    memcpy(vchAmount.data(), &amount, 8);

    OUTPUT_PTR<CTxOutStandard> out = MAKE_OUTPUT<CTxOutStandard>();
    out->nValue = amount;
    out->scriptPubKey = prevScriptPK;

    tx.vpout.push_back(out);

    CHashWriter ss(SER_GETHASH, 0);
    ss << tx.nVersion;
    ss << GetPrevoutHash(tx);
    ss << GetSequenceHash(tx);
    ss << tx.vin[0].prevout;
    ss << tx.vin[0].scriptSig;
    if (!vchAmount.empty()) {
        ss.write((const char*)&vchAmount[0], vchAmount.size());
    }
    ss << tx.vin[0].nSequence;
    ss << GetOutputsHash(tx);
    ss << tx.nLockTime;
    ss << (int)SIGHASH_ALL;

    uint256 expectedHash = ss.GetHash();

    uint256 hash = SignatureHash(prevScriptPK, tx, 0, SIGHASH_ALL, vchAmount, SIGVERSION_WITNESS_V0);
    BOOST_CHECK_EQUAL(hash, expectedHash);
}

BOOST_AUTO_TEST_SUITE_END()
