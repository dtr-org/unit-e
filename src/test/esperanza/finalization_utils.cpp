#include <keystore.h>
#include <random.h>
#include <test/esperanza/finalization_utils.h>
#include <boost/test/unit_test.hpp>

CTransaction CreateBaseTransaction(const CTransaction &spendableTx,
                                   const CKey &spendableKey, CAmount amount,
                                   const TxType type,
                                   const CScript &scriptPubKey) {

  CBasicKeyStore keystore;
  keystore.AddKey(spendableKey);

  CMutableTransaction mutTx;
  mutTx.SetType(type);

  mutTx.vin.resize(1);
  mutTx.vin[0].prevout.hash = spendableTx.GetHash();
  mutTx.vin[0].prevout.n = 0;

  CTxOut out(amount, scriptPubKey);
  mutTx.vout.push_back(out);

  // Sign
  std::vector<unsigned char> vchSig;
  uint256 hash = SignatureHash(spendableTx.vout[0].scriptPubKey, mutTx, 0,
                               SIGHASH_ALL, amount, SIGVERSION_BASE);

  BOOST_CHECK(spendableKey.Sign(hash, vchSig));
  vchSig.push_back((unsigned char)SIGHASH_ALL);

  mutTx.vin[0].scriptSig = CScript() << ToByteVector(vchSig)
                                     << ToByteVector(spendableKey.GetPubKey());

  return CTransaction(mutTx);
}

CTransaction CreateVoteTx(esperanza::Vote &vote) {

  CMutableTransaction mutTx;
  mutTx.SetType(TxType::VOTE);

  mutTx.vin.resize(1);
  uint256 signature = GetRandHash();

  CScript encodedVote = CScript::EncodeVote(vote);
  std::vector<unsigned char> voteVector(encodedVote.begin(), encodedVote.end());

  CScript voteScript = (CScript() << ToByteVector(signature)) << voteVector;
  mutTx.vin[0] = (CTxIn(GetRandHash(), 0, voteScript));

  uint256 keyHash = GetRandHash();
  CKey k;
  k.Set(keyHash.begin(), keyHash.end(), true);
  CScript scriptPubKey = CScript::CreatePayVoteSlashScript(k.GetPubKey());

  CTxOut out{10000, scriptPubKey};
  mutTx.vout.push_back(out);

  return CTransaction(mutTx);
}

CTransaction CreateDepositTx(const CTransaction &spendableTx,
                             const CKey &spendableKey, CAmount amount) {
  CScript scriptPubKey =
      CScript::CreatePayVoteSlashScript(spendableKey.GetPubKey());

  return CreateBaseTransaction(spendableTx, spendableKey, amount,
                               TxType::DEPOSIT, scriptPubKey);
}

CTransaction CreateLogoutTx(const CTransaction &spendableTx,
                            const CKey &spendableKey, CAmount amount) {

  CScript scriptPubKey =
      CScript::CreatePayVoteSlashScript(spendableKey.GetPubKey());

  return CreateBaseTransaction(spendableTx, spendableKey, amount,
                               TxType::LOGOUT, scriptPubKey);
}

CTransaction CreateWithdrawTx(const CTransaction &spendableTx,
                              const CKey &spendableKey, CAmount amount) {

  CScript scriptPubKey = CScript::CreateP2PKHScript(
      ToByteVector(spendableKey.GetPubKey().GetID()));

  return CreateBaseTransaction(spendableTx, spendableKey, amount,
                               TxType::WITHDRAW, scriptPubKey);
}

CTransaction CreateP2PKHTx(const CTransaction &spendableTx,
                           const CKey &spendableKey, CAmount amount) {

  CScript scriptPubKey = CScript::CreateP2PKHScript(
      ToByteVector(spendableKey.GetPubKey().GetID()));

  return CreateBaseTransaction(spendableTx, spendableKey, amount,
                               TxType::STANDARD, scriptPubKey);
}
