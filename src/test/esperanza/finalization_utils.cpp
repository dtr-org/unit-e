// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keystore.h>
#include <random.h>
#include <test/esperanza/finalization_utils.h>
#include <boost/test/unit_test.hpp>

CTransaction CreateBaseTransaction(const CTransaction &spendable_tx,
                                   const CKey &spendable_key,
                                   const CAmount &amount,
                                   const TxType type,
                                   const CScript &script_pub_key,
                                   const CAmount &change = 0) {

  CBasicKeyStore keystore;
  keystore.AddKey(spendable_key);

  CMutableTransaction mtx;
  mtx.SetType(type);

  mtx.vin.resize(1);
  mtx.vin[0].prevout.hash = spendable_tx.GetHash();
  mtx.vin[0].prevout.n = 0;

  CAmount value_out = amount - change;
  mtx.vout.emplace_back(value_out, script_pub_key);

  if (change > 0) {
    mtx.vout.emplace_back(change, script_pub_key);
  }

  // Sign
  std::vector<unsigned char> vch_sig;
  uint256 hash = SignatureHash(spendable_tx.vout[0].scriptPubKey, mtx, 0,
                               SIGHASH_ALL, amount, SigVersion::BASE);

  BOOST_CHECK(spendable_key.Sign(hash, vch_sig));
  vch_sig.push_back((unsigned char)SIGHASH_ALL);

  mtx.vin[0].scriptSig = CScript() << ToByteVector(vch_sig)
                                   << ToByteVector(spendable_key.GetPubKey());

  return CTransaction(mtx);
}

CTransaction CreateVoteTx(const CTransaction &spendable_tx, const CKey &spendable_key,
                          const esperanza::Vote &vote, const std::vector<unsigned char> &vote_sig) {

  CMutableTransaction mtx;
  mtx.SetType(TxType::VOTE);
  mtx.vin.resize(1);
  mtx.vout.resize(1);

  CScript vote_script = CScript::EncodeVote(vote, vote_sig);
  std::vector<unsigned char> voteVector(vote_script.begin(), vote_script.end());

  CScript script_sig = (CScript() << vote_sig) << voteVector;
  mtx.vin[0] = CTxIn(GetRandHash(), 0, script_sig);

  CScript script_pub_key = CScript::CreatePayVoteSlashScript(spendable_key.GetPubKey());
  mtx.vout[0] = CTxOut(10000, script_pub_key);

  mtx.vin[0].prevout.hash = spendable_tx.GetHash();
  mtx.vin[0].prevout.n = 0;

  return CTransaction(mtx);
}

CTransaction CreateVoteTx(const esperanza::Vote &vote, const CKey &spendable_key) {

  CTransaction spendable_tx;

  std::vector<unsigned char> vote_sig;
  BOOST_CHECK(spendable_key.Sign(vote.GetHash(), vote_sig));

  return CreateVoteTx(spendable_tx, spendable_key, vote, vote_sig);
}

CTransaction CreateDepositTx(const CTransaction &spendable_tx,
                             const CKey &spendable_key,
                             const CAmount &amount,
                             const CAmount &change) {

  CScript script_pub_key =
      CScript::CreatePayVoteSlashScript(spendable_key.GetPubKey());

  return CreateBaseTransaction(spendable_tx, spendable_key, amount,
                               TxType::DEPOSIT, script_pub_key, change);
}

CTransaction CreateLogoutTx(const CTransaction &spendable_tx,
                            const CKey &spendable_key, CAmount amount) {

  CScript script_pub_key =
      CScript::CreatePayVoteSlashScript(spendable_key.GetPubKey());

  return CreateBaseTransaction(spendable_tx, spendable_key, amount,
                               TxType::LOGOUT, script_pub_key);
}

CTransaction CreateWithdrawTx(const CTransaction &spendable_tx,
                              const CKey &spendable_key, CAmount amount) {

  CScript script_pub_key = CScript::CreateP2PKHScript(
      ToByteVector(spendable_key.GetPubKey().GetID()));

  return CreateBaseTransaction(spendable_tx, spendable_key, amount,
                               TxType::WITHDRAW, script_pub_key);
}

CTransaction CreateP2PKHTx(const CTransaction &spendable_tx,
                           const CKey &spendable_key, CAmount amount) {

  CScript script_pub_key = CScript::CreateP2PKHScript(
      ToByteVector(spendable_key.GetPubKey().GetID()));

  return CreateBaseTransaction(spendable_tx, spendable_key, amount,
                               TxType::REGULAR, script_pub_key);
}
