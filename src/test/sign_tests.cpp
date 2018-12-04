// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
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

  esperanza::Vote vote{pk.GetID(), GetRandHash(), 10, 100};

  //we don't need to test here the signing process, that is already tested elsewhere
  std::vector<unsigned char> voteSig;
  BOOST_CHECK(k.Sign(GetRandHash(), voteSig));

  CScript voteScript = CScript::EncodeVote(vote, voteSig);
  txn.vin.push_back(CTxIn(GetRandHash(), 0, voteScript, CTxIn::SEQUENCE_FINAL));

  const CScript &prevScriptPubKey = CScript::CreatePayVoteSlashScript(pk);
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

  esperanza::Vote signedVote;
  std::vector<unsigned char> extractedVoteSig;
  BOOST_CHECK(CScript::ExtractVoteFromVoteSignature(sigdata.scriptSig, signedVote, extractedVoteSig));

  BOOST_CHECK_EQUAL(vote.m_validatorAddress.GetHex(), signedVote.m_validatorAddress.GetHex());
  BOOST_CHECK_EQUAL(vote.m_targetHash, signedVote.m_targetHash);
  BOOST_CHECK_EQUAL(vote.m_sourceEpoch, signedVote.m_sourceEpoch);
  BOOST_CHECK_EQUAL(vote.m_targetEpoch, signedVote.m_targetEpoch);
}

BOOST_AUTO_TEST_CASE(producesignature_logout) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  CMutableTransaction txn;
  txn.SetType(TxType::LOGOUT);

  CScript scriptSig = CScript() << ToByteVector(pk);
  txn.vin.push_back(CTxIn(GetRandHash(), 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  const CScript& prevScriptPubKey = CScript::CreatePayVoteSlashScript(pk);
  const CAmount amount = 10000000;
  CTxOut txout(amount, prevScriptPubKey);

  txn.vout.push_back(txout);

  CTransaction txToConst(txn);
  SignatureData sigdata;

  uint32_t nIn = 0;
  std::string strFailReason;

  BOOST_CHECK(ProduceSignature(
      TransactionSignatureCreator(&keystore, &txToConst, nIn,
                                  amount, SIGHASH_ALL),
      prevScriptPubKey, sigdata, &txToConst));

  txn.vin[0].scriptSig = sigdata.scriptSig + scriptSig;

  ScriptError serror;
  BOOST_CHECK(VerifyScript(
      txn.vin[0].scriptSig, prevScriptPubKey, &sigdata.scriptWitness,
      STANDARD_SCRIPT_VERIFY_FLAGS,
      TransactionSignatureChecker(&txToConst, 0, amount), &serror));

  BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, serror);
}

BOOST_AUTO_TEST_CASE(producesignature_withdraw) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  CMutableTransaction txn;
  txn.SetType(TxType::WITHDRAW);

  CScript scriptSig = CScript() << ToByteVector(pk);
  txn.vin.push_back(CTxIn(GetRandHash(), 0, scriptSig, CTxIn::SEQUENCE_FINAL));

  const CScript& prevScriptPubKey = CScript::CreatePayVoteSlashScript(pk);
  const CScript& scriptPubKey = CScript::CreateP2PKHScript(ToByteVector(pk.GetID()));
  const CAmount amount = 10000000;
  CTxOut txout(amount, scriptPubKey);

  txn.vout.push_back(txout);

  CTransaction txToConst(txn);
  SignatureData sigdata;

  uint32_t nIn = 0;
  std::string strFailReason;

  BOOST_CHECK(ProduceSignature(
      TransactionSignatureCreator(&keystore, &txToConst, nIn,
                                  amount, SIGHASH_ALL),
      prevScriptPubKey, sigdata, &txToConst));

  txn.vin[0].scriptSig = sigdata.scriptSig + scriptSig;

  ScriptError serror;
  BOOST_CHECK(VerifyScript(
      txn.vin[0].scriptSig, prevScriptPubKey, &sigdata.scriptWitness,
      STANDARD_SCRIPT_VERIFY_FLAGS,
      TransactionSignatureChecker(&txToConst, 0, amount), &serror));

  BOOST_CHECK_EQUAL(SCRIPT_ERR_OK, serror);
}

BOOST_AUTO_TEST_SUITE_END()
