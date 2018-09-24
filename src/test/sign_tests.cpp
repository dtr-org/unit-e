#include <esperanza/vote.h>
#include <keystore.h>
#include <policy/policy.h>
#include <script/sign.h>
#include <string.h>
#include <test/test_unite.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

BOOST_FIXTURE_TEST_SUITE(sign_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(producesignature_vote) {
  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  CMutableTransaction txn;
  txn.SetType(TxType::VOTE);
  txn.nLockTime = 0;

  esperanza::Vote vote{GetRandHash(), GetRandHash(), 10, 100};
  CScript voteScript = CScript::EncodeVote(vote);
  txn.vin.push_back(CTxIn(GetRandHash(), 0, voteScript, CTxIn::SEQUENCE_FINAL));

  const CScript& prevScriptPubKey = CScript::CreatePayVoteSlashScript(pk);
  const CAmount amount = 10000000;

  CTxOut out(amount, prevScriptPubKey);
  txn.vout.push_back(out);

  CTransaction txToConst(txn);
  SignatureData sigdata;

  BOOST_CHECK(
      ProduceSignature(TransactionSignatureCreator(&keystore, &txToConst, 0,
                                                   amount, SIGHASH_ALL),
                       prevScriptPubKey, sigdata, &txToConst));

  ScriptError serror;
  BOOST_CHECK(VerifyScript(sigdata.scriptSig, prevScriptPubKey,
                           &sigdata.scriptWitness, STANDARD_SCRIPT_VERIFY_FLAGS,
                           TransactionSignatureChecker(&txToConst, 0, amount),
                           &serror));

  BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, serror);

  esperanza::Vote signedVote =
      CScript::ExtractVoteFromSignature(sigdata.scriptSig);

  BOOST_CHECK_EQUAL(vote.m_validatorIndex, signedVote.m_validatorIndex);
  BOOST_CHECK_EQUAL(vote.m_targetHash, signedVote.m_targetHash);
  BOOST_CHECK_EQUAL(vote.m_sourceEpoch, signedVote.m_sourceEpoch);
  BOOST_CHECK_EQUAL(vote.m_targetEpoch, signedVote.m_targetEpoch);
}

BOOST_AUTO_TEST_SUITE_END()
