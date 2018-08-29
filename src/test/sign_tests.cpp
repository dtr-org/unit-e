#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <test/test_bitcoin.h>
#include <util.h>
#include <script/sign.h>
#include <wallet/hdwallet.h>
#include <policy/policy.h>
#include <string.h>
#include <core_io.h>

BOOST_FIXTURE_TEST_SUITE(sign_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(producesignature_vote_witness)
{

    SeedInsecureRand();
    CBasicKeyStore keystore;

    CKey k;
    InsecureNewKey(k, true);
    keystore.AddKey(k);

    CPubKey pk = k.GetPubKey();

    CMutableTransaction txn;
    txn.nVersion = PARTICL_TXN_VERSION;
    txn.SetType(TXN_VOTE);
    txn.nLockTime = 0;


    VoteData vote {GetRandHash(), GetRandHash(), 10, 100};
    CScript voteScript = CScript::EncodeVoteData(vote);
    txn.vin.push_back(CTxIn(GetRandHash(), 0, voteScript, CTxIn::SEQUENCE_FINAL));

    OUTPUT_PTR<CTxOutBase> txbout;

    const CScript& prevScriptPubKey = CScript::CreatePayVoteSlashScript(pk);
    const CAmount amount = 10000000;

    CTempRecipient tr;
    tr.scriptPubKey = prevScriptPubKey;
    tr.SetAmount(amount);
    tr.nType = OUTPUT_VOTE;
    std::string strError;

    BOOST_CHECK_EQUAL(0, CreateOutput(txbout, tr, strError));

    txn.vpout.push_back(txbout);

    std::vector<uint8_t> vchAmount(8);
    memcpy(&vchAmount[0], &amount, 8);

    CTransaction txToConst(txn);
    SignatureData sigdata;
    BOOST_CHECK(ProduceSignature(TransactionSignatureCreator(&keystore, &txToConst, 0, vchAmount, SIGHASH_ALL), prevScriptPubKey, sigdata, &txToConst));

    ScriptError serror;
    BOOST_CHECK(VerifyScript(sigdata.scriptSig, prevScriptPubKey, &sigdata.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txToConst, 0, vchAmount), &serror));
    BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, serror);
}

BOOST_AUTO_TEST_SUITE_END()
