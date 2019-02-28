// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_parameters.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

#include <amount.h>
#include <blockchain/blockchain_types.h>
#include <key.h>
#include <keystore.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/ismine.h>
#include <util.h>

#include <array>
#include <functional>

namespace {

struct Txs {
  CTransaction funding_tx;
  CTransaction spending_tx;
};

template <unsigned int NumKeys>
class ExtractBlockSigningKeyFixture {

 private:
  //! just some value to use for transaction amounts
  CAmount amount = 10 * UNIT;

  //! a simple keystore (a CWallet is a KeyStore)
  CBasicKeyStore keystore;

  //! private keys
  std::array<CKey, NumKeys> keys;

  //! public keys
  std::vector<CPubKey> pubkeys;

  //! any parameters suffice, ExtractBlockSigningKeys does not depend on any
  blockchain::Parameters parameters;

  //! an instance of blockchain::Behavior for checking ExtractBlockSigningKeys
  std::unique_ptr<blockchain::Behavior> blockchain_behavior =
      blockchain::Behavior::NewFromParameters(parameters);

 public:
  CKeyStore &GetKeyStore() { return keystore; }
  blockchain::Behavior &GetBlockchainBehavior() { return *blockchain_behavior; }
  const std::vector<CPubKey> &GetPubKeys() { return pubkeys; }

  //! Initiate a fixture that has NumKeys amount of keys prepared in its keystore.
  ExtractBlockSigningKeyFixture() {
    for (auto &key : keys) {
      key.MakeNewKey(/* fCompressed= */ true);
      keystore.AddKey(key);
    }
    pubkeys.resize(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
      pubkeys[i] = keys[i].GetPubKey();
    }
  }

  //! Create a P2WPKH Transaction.
  const Txs GetP2WPKHTransaction(const CPubKey &pubkey) {

    const CTxDestination destination = WitnessV0KeyHash(pubkey.GetID());
    const CScript p2wpkh_script = GetScriptForDestination(destination);

    CMutableTransaction mutable_funding_tx;
    mutable_funding_tx.vout.emplace_back(amount, p2wpkh_script);

    const CTransaction funding_tx(mutable_funding_tx);
    BOOST_CHECK_EQUAL(isminetype::ISMINE_SPENDABLE, IsMine(GetKeyStore(), funding_tx.vout[0].scriptPubKey));
    std::string error;
    BOOST_CHECK(IsStandardTx(funding_tx, error, /* witnessEnabled= */ true));

    CMutableTransaction mutable_spending_tx;
    const uint256 funding_tx_hash = funding_tx.GetHash();
    mutable_spending_tx.vin.emplace_back(funding_tx_hash, 0);
    mutable_spending_tx.vout.emplace_back(amount, p2wpkh_script);

    BOOST_REQUIRE(SignSignature(GetKeyStore(), funding_tx, mutable_spending_tx, 0, SIGHASH_ALL));

    const CTransaction spending_tx(mutable_spending_tx);
    return {funding_tx, spending_tx};
  }

  //! Create a P2WSH Transaction that wraps the given script.
  const Txs GetP2WSHTransaction(const CScript &inner_script) {

    GetKeyStore().AddCScript(inner_script);
    uint256 inner_script_hash;

    CSHA256().Write(&inner_script[0], inner_script.size()).Finalize(inner_script_hash.begin());
    const CTxDestination destination = WitnessV0ScriptHash(inner_script_hash);
    const CScript p2wsh_script = GetScriptForDestination(destination);
    GetKeyStore().AddCScript(p2wsh_script);

    CMutableTransaction mutable_funding_tx;
    mutable_funding_tx.vout.emplace_back(amount, p2wsh_script);

    const CTransaction funding_tx(mutable_funding_tx);
    BOOST_CHECK_EQUAL(isminetype::ISMINE_SPENDABLE, IsMine(GetKeyStore(), funding_tx.vout[0].scriptPubKey));
    std::string error;
    BOOST_CHECK(IsStandardTx(funding_tx, error, /* witnessEnabled= */ true));

    CMutableTransaction mutable_spending_tx;
    const uint256 funding_tx_hash = funding_tx.GetHash();
    mutable_spending_tx.vin.emplace_back(funding_tx_hash, 0);
    mutable_spending_tx.vout.emplace_back(amount, p2wsh_script);

    BOOST_CHECK(SignSignature(GetKeyStore(), funding_tx, mutable_spending_tx, 0, SIGHASH_ALL));

    const CTransaction spending_tx(mutable_spending_tx);
    return {funding_tx, spending_tx};
  }

  //! Check extracted keys against the pubkeys in this fixture
  //!
  //! Slightly more complicated than merely doing BOOST_CHECK_EQUAL as CPubKey
  //! needs to be serialized ToString() first.
  void CheckExtractedKeys(const std::vector<CPubKey> &extracted_pubkeys) {

    std::vector<std::string> stringified_pubkeys(pubkeys.size());
    std::vector<std::string> stringified_extracted_pubkeys(extracted_pubkeys.size());

    const auto pubkey_to_string = [](const CPubKey &key) { return key.GetID().ToString(); };
    std::transform(pubkeys.begin(), pubkeys.end(),
                   stringified_pubkeys.begin(), pubkey_to_string);
    std::transform(extracted_pubkeys.begin(), extracted_pubkeys.end(),
                   stringified_extracted_pubkeys.begin(), pubkey_to_string);

    BOOST_CHECK_EQUAL(stringified_extracted_pubkeys, stringified_pubkeys);
  }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(blockchain_behavior_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wpkh) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  const Txs txs = fixture.GetP2WPKHTransaction(fixture.GetPubKeys()[0]);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  fixture.CheckExtractedKeys(extracted_pubkeys);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_pubkey) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // a script that spends directly to a pubkey
  const CScript public_key_script = GetScriptForRawPubKey(fixture.GetPubKeys()[0]);
  const Txs txs = fixture.GetP2WSHTransaction(public_key_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  fixture.CheckExtractedKeys(extracted_pubkeys);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_pubkeyhash) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // a script that spends to a pubkeyhash
  const CScript public_key_script = GetScriptForDestination(fixture.GetPubKeys()[0].GetID());
  const Txs txs = fixture.GetP2WSHTransaction(public_key_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  fixture.CheckExtractedKeys(extracted_pubkeys);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_multisig_one_of_one) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // create a 1-of-1 multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  fixture.CheckExtractedKeys(extracted_pubkeys);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_multisig_one_of_four) {
  // a fixture with four keys
  ExtractBlockSigningKeyFixture<4> fixture;
  // create a 1-of-4 multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  fixture.CheckExtractedKeys(extracted_pubkeys);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_multisig_two_of_four) {
  // multisig p2wsh transactions that require more than one signature
  // are not supported for staking as only one single proposer can
  // stake (and therefore sign) the newly proposed block + staking input.

  // a fixture with four keys
  ExtractBlockSigningKeyFixture<4> fixture;
  // create a 2-of-4 multisig tx
  const CScript multisig_script = GetScriptForMultisig(2, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  BOOST_CHECK(!IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check that no pubkey was extracted
  BOOST_CHECK_EQUAL(extracted_pubkeys.size(), 0);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wpkh_malformed) {
  // a fixture with one keys
  ExtractBlockSigningKeyFixture<1> fixture;
  const Txs txs = fixture.GetP2WPKHTransaction(fixture.GetPubKeys()[0]);
  // mutilate the pubkey
  CMutableTransaction mutable_spending_tx = CMutableTransaction(txs.spending_tx);
  mutable_spending_tx.vin[0].scriptWitness.stack[1].emplace_back('\x7a');
  const CTransaction spending_tx(mutable_spending_tx);
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(spending_tx.vin[0]);
  // check that no pubkey was extracted
  BOOST_CHECK_EQUAL(extracted_pubkeys.size(), 0);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_malformed) {
  // a fixture with two keys
  ExtractBlockSigningKeyFixture<2> fixture;
  // create a 1-of-2 multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  // mutilate the pubkyes in the witnessScript
  CMutableTransaction mutable_spending_tx = CMutableTransaction(txs.spending_tx);
  std::vector<unsigned char> &serialized_script = mutable_spending_tx.vin[0].scriptWitness.stack[2];
  // increment the size indicator of the first pubkey
  serialized_script[1] += 1;
  // insert some junk in that pubkey
  serialized_script.insert(serialized_script.begin() + 10, '\x03');
  // seal the transaction with the mutilated script
  const CTransaction spending_tx(mutable_spending_tx);
  // re-learn that script
  fixture.GetKeyStore().AddCScript(CScript(serialized_script));
  // the script is still stakeable as there is a valid pubkey remaining
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys =
      fixture.GetBlockchainBehavior().ExtractBlockSigningKeys(spending_tx.vin[0]);
  // check that only one (the non-corrupted pubkey) was extracted
  BOOST_CHECK_EQUAL(extracted_pubkeys.size(), 1);
}

BOOST_AUTO_TEST_SUITE_END()
