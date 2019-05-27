// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/proof_of_stake.h>

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
#include <util/system.h>

#include <array>
#include <functional>

namespace {

struct Txs {
  CTransaction funding_tx;
  CTransaction spending_tx;
};

//! just some value to use for transaction amounts
constexpr CAmount some_amount = 10 * UNIT;

template <unsigned int NumKeys>
class ExtractBlockSigningKeyFixture {

 private:
  //! a simple keystore (a CWallet is a KeyStore)
  CBasicKeyStore keystore;

  //! private keys
  std::array<CKey, NumKeys> keys;

  //! public keys
  std::vector<CPubKey> pubkeys;

 public:
  CKeyStore &GetKeyStore() { return keystore; }
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
    mutable_funding_tx.vout.emplace_back(some_amount, p2wpkh_script);

    const CTransaction funding_tx(mutable_funding_tx);
    BOOST_CHECK_EQUAL(isminetype::ISMINE_SPENDABLE, IsMine(GetKeyStore(), funding_tx.vout[0].scriptPubKey));
    std::string error;
    BOOST_CHECK(IsStandardTx(funding_tx, error));

    CMutableTransaction mutable_spending_tx;
    const uint256 funding_tx_hash = funding_tx.GetHash();
    mutable_spending_tx.vin.emplace_back(funding_tx_hash, 0);
    mutable_spending_tx.vout.emplace_back(some_amount, p2wpkh_script);

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
    mutable_funding_tx.vout.emplace_back(some_amount, p2wsh_script);

    const CTransaction funding_tx(mutable_funding_tx);
    BOOST_CHECK_EQUAL(isminetype::ISMINE_SPENDABLE, IsMine(GetKeyStore(), funding_tx.vout[0].scriptPubKey));
    std::string error;
    BOOST_CHECK(IsStandardTx(funding_tx, error));

    CMutableTransaction mutable_spending_tx;
    const uint256 funding_tx_hash = funding_tx.GetHash();
    mutable_spending_tx.vin.emplace_back(funding_tx_hash, 0);
    mutable_spending_tx.vout.emplace_back(some_amount, p2wsh_script);

    BOOST_CHECK(SignSignature(GetKeyStore(), funding_tx, mutable_spending_tx, 0, SIGHASH_ALL));

    const CTransaction spending_tx(mutable_spending_tx);
    return {funding_tx, spending_tx};
  }
};

}  // namespace

BOOST_FIXTURE_TEST_SUITE(proof_of_stake_tests, ReducedTestingSetup)

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wpkh) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  const Txs txs = fixture.GetP2WPKHTransaction(fixture.GetPubKeys()[0]);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  BOOST_CHECK_EQUAL(extracted_pubkeys, fixture.GetPubKeys());
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_pubkey) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // a script that spends directly to a pubkey
  const CScript public_key_script = GetScriptForRawPubKey(fixture.GetPubKeys()[0]);
  const Txs txs = fixture.GetP2WSHTransaction(public_key_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  BOOST_CHECK_EQUAL(extracted_pubkeys, fixture.GetPubKeys());
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_pubkeyhash) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // a script that spends to a pubkeyhash
  const CScript public_key_script = GetScriptForDestination(fixture.GetPubKeys()[0].GetID());
  const Txs txs = fixture.GetP2WSHTransaction(public_key_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  BOOST_CHECK_EQUAL(extracted_pubkeys, fixture.GetPubKeys());
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_multisig_one_of_one) {
  // a fixture with one key
  ExtractBlockSigningKeyFixture<1> fixture;
  // create a 1-of-1 multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  BOOST_CHECK_EQUAL(extracted_pubkeys, fixture.GetPubKeys());
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_multisig_one_of_four) {
  // a fixture with four keys
  ExtractBlockSigningKeyFixture<4> fixture;
  // create a 1-of-4 multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  BOOST_CHECK(IsStakeableByMe(fixture.GetKeyStore(), txs.funding_tx.vout[0].scriptPubKey));
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
  // check against all the keys in the fixture
  BOOST_CHECK_EQUAL(extracted_pubkeys, fixture.GetPubKeys());
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
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(txs.spending_tx.vin[0]);
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
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(spending_tx.vin[0]);
  // check that no pubkey was extracted
  BOOST_CHECK_EQUAL(extracted_pubkeys.size(), 0);
}

template <unsigned int NumKeys>
void MalformedP2WSHTestCase(std::function<void(std::vector<unsigned char> &)> mutilator) {
  // a fixture with NumKeys keys
  ExtractBlockSigningKeyFixture<NumKeys> fixture;
  // create a 1-of-NumKeys multisig tx
  const CScript multisig_script = GetScriptForMultisig(1, fixture.GetPubKeys());
  const Txs txs = fixture.GetP2WSHTransaction(multisig_script);
  // mutilate the witnessScript
  CMutableTransaction mutable_spending_tx = CMutableTransaction(txs.spending_tx);
  std::vector<unsigned char> &serialized_script = mutable_spending_tx.vin[0].scriptWitness.stack[2];
  // let the mutilator mutilate the script
  mutilator(serialized_script);
  // seal the transaction with the mutilated script
  const CTransaction spending_tx(mutable_spending_tx);
  const std::vector<CPubKey> extracted_pubkeys = staking::ExtractBlockSigningKeys(spending_tx.vin[0]);
  // check that the expected number of pubkeys was extracted
  BOOST_CHECK_EQUAL(extracted_pubkeys.size(), 0);
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_malformed) {
  MalformedP2WSHTestCase<2>([](std::vector<unsigned char> &serialized_script) {
    // at serialized_script[1] the length of the pubkey (33) should be recorded
    BOOST_REQUIRE_EQUAL(serialized_script[1], 33);
    // increment the size indicator of the first pubkey
    serialized_script[1] += 1;
    // insert some junk in that pubkey
    serialized_script.insert(serialized_script.begin() + 10, '\x03');
  });
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_witness_script_malformed_too_many_pubkeys) {
  MalformedP2WSHTestCase<3>([](std::vector<unsigned char> &serialized_script) {
    // the serialized script in stack[2] should have a 3 (encoded as OP_3) at the second index before the end:
    BOOST_REQUIRE_EQUAL(*(serialized_script.end() - 2), OP_3);
    // decrement the number of public keys that need to be provided
    *(serialized_script.end() - 2) = OP_2;
  });
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_witness_script_malformed_too_few_pubkeys) {
  MalformedP2WSHTestCase<3>([](std::vector<unsigned char> &serialized_script) {
    // the serialized script in stack[2] should have a 3 (encoded as OP_3) at the second index before the end:
    BOOST_REQUIRE_EQUAL(*(serialized_script.end() - 2), OP_3);
    // increment the number of public keys that need to be provided
    *(serialized_script.end() - 2) = OP_4;
  });
}

BOOST_AUTO_TEST_CASE(extract_block_signing_key_p2wsh_witness_script_malformed_missing_OP_CHECKMULTISIG) {
  MalformedP2WSHTestCase<3>([](std::vector<unsigned char> &serialized_script) {
    // remove OP_CHECKMULTISIG which is the last opcode in the script
    serialized_script.resize(serialized_script.size() - 1);
  });
}

BOOST_AUTO_TEST_SUITE_END()
