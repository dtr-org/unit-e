// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <base58.h>
#include <blockchain/blockchain_behavior.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <esperanza/finalizationparams.h>
#include <esperanza/vote.h>
#include <esperanza/walletextension.h>
#include <key.h>
#include <keystore.h>
#include <primitives/transaction.h>
#include <primitives/txtype.h>
#include <proposer/block_builder.h>
#include <proposer/eligible_coin.h>
#include <script/script.h>
#include <staking/coin.h>
#include <test/esperanza/finalization_utils.h>
#include <txmempool.h>
#include <validation.h>

#include <test/test_unite.h>
#include <wallet/test/wallet_test_fixture.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(walletextension_tests)

BOOST_FIXTURE_TEST_CASE(vote_signature, ReducedTestingSetup) {

  CBasicKeyStore keystore;
  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  esperanza::Vote vote{pk.GetID(), GetRandHash(), 10, 100};
  std::vector<unsigned char> voteSig;
  BOOST_CHECK(esperanza::Vote::CreateSignature(&keystore, vote, voteSig));

  std::vector<unsigned char> expectedSig;
  k.Sign(vote.GetHash(), expectedSig);

  BOOST_CHECK_EQUAL(HexStr(expectedSig), HexStr(voteSig));
  BOOST_CHECK(esperanza::Vote::CheckSignature(pk, vote, voteSig));
}

BOOST_FIXTURE_TEST_CASE(vote_signature2, TestingSetup) {

  CBasicKeyStore keystore;

  auto params = blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet());

  CTxDestination dest = DecodeDestination("muUx4dQ4bwssNQYpUqAJHSJCUonAZ4Ro2s", *params);
  const CKeyID *keyID = boost::get<CKeyID>(&dest);

  esperanza::Vote vote{*keyID, uint256S("4e7eae1625c033a05e92cff8d1591e4c7511888c264dbc8917ef94c3e66f22ef"), 12, 13};

  std::string pkey = "cNJWVLVrfrxZT85cwYfHdbRKGi2FQjkKFBjocwwinNNix5tytG33";

  CUnitESecret vchSecret;
  vchSecret.SetString(pkey);
  CKey key = vchSecret.GetKey();
  keystore.AddKey(key);

  std::vector<unsigned char> voteSig;
  esperanza::Vote::CreateSignature(&keystore, vote, voteSig);
}

BOOST_FIXTURE_TEST_CASE(sign_coinbase_transaction, WalletTestingSetup) {

  const key::mnemonic::Seed seed("stizzoso atavico inodore srotolato birra stupendo velina incendio copione pietra alzare privato folata madama gemmato");
  const CExtKey &ext_key = seed.GetExtKey();

  const auto key = ext_key.key;
  const auto pubkey = key.GetPubKey();
  const auto pubkeydata = std::vector<unsigned char>(pubkey.begin(), pubkey.end());

  auto behavior = blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet());
  auto block_builder = proposer::BlockBuilder::New(behavior.get(), &settings);

  {
    LOCK(pwalletMain->cs_wallet);
    pwalletMain->AddKeyPubKey(key, pubkey);
  }

  const auto destination = WitnessV0KeyHash(pubkey.GetID());

  CMutableTransaction tx1;
  tx1.vout.emplace_back(100, GetScriptForDestination(destination));

  CMutableTransaction tx2;
  tx2.vout.emplace_back(1250, GetScriptForDestination(destination));

  CMutableTransaction tx3;
  tx3.vout.emplace_back(125, GetScriptForDestination(destination));

  CTransactionRef tx1ref = MakeTransactionRef(tx1);
  CTransactionRef tx2ref = MakeTransactionRef(tx2);
  CTransactionRef tx3ref = MakeTransactionRef(tx3);

  {
    LOCK(pwalletMain->cs_wallet);
    const CWallet *wallet = pwalletMain.get();

    CWalletTx walletTx1(wallet, tx1ref);
    CWalletTx walletTx2(wallet, tx2ref);
    CWalletTx walletTx3(wallet, tx3ref);

    pwalletMain->LoadToWallet(walletTx1);
    pwalletMain->LoadToWallet(walletTx2);
    pwalletMain->LoadToWallet(walletTx3);
  }

  CScript prev_script_pubkey = CScript::CreateP2PKHScript(ToByteVector(pubkey.GetID()));
  staking::Coin coin1{
      tx1ref->GetHash(),   // txid
      0,                   // index
      100,                 // amount
      prev_script_pubkey,  // scriptpubkey
      230                  // depth
  };
  staking::Coin coin2{
      tx2ref->GetHash(),   // txid
      0,                   // index
      1250,                // amount
      prev_script_pubkey,  // scriptpubkey
      230                  // depth

  };
  staking::Coin coin3{
      tx3ref->GetHash(),   // txid
      0,                   // index
      125,                 // amount
      prev_script_pubkey,  // scriptpubkey
      230                  // depth
  };
  proposer::EligibleCoin eligible_coin{
      coin2,       // coin used as stake
      uint256(),   // kernel hash
      5000,        // reward
      7251,        // target height
      1548255362,  // target time,
      0x1d00ffff   // difficulty = 1
  };

  std::vector<staking::Coin> coins{coin1, coin2, coin3};

  // BuildCoinbaseTransaction() will also sign it
  CTransactionRef coinbase_transaction =
      block_builder->BuildCoinbaseTransaction(uint256(), eligible_coin, coins, 700, pwalletMain->GetWalletExtension());

  // check that a coinbase transaction was built successfully
  BOOST_REQUIRE(static_cast<bool>(coinbase_transaction));

  // should contain:
  // - (0) meta input
  // - (1) coin2 (the stake, eligible_coin uses coin2)
  // - (2) coin1 (combined other coin)
  // - (3) coin3 (combined other coin)
  BOOST_REQUIRE_EQUAL(coinbase_transaction->vin.size(), 4);

  // (0) meta input need not be signed
  BOOST_CHECK(coinbase_transaction->vin[0].scriptWitness.IsNull());

  // (1, 2, 3) remaining pieces must be signed with pubkey
  for (int i = 1; i < 3; ++i) {
    auto &stack = coinbase_transaction->vin[1].scriptWitness.stack;
    BOOST_CHECK_EQUAL(stack.size(), 2);  // signature + public key
    auto &witness_pubkey = stack[1];
    BOOST_CHECK(witness_pubkey == pubkeydata);
  }

  // We should be able to spend all the outputs
  for (const auto &out : coinbase_transaction->vout) {
    BOOST_CHECK(::IsMine(*pwalletMain, out.scriptPubKey) == ISMINE_SPENDABLE);
  }
}

BOOST_FIXTURE_TEST_CASE(get_remote_staking_balance, WalletTestingSetup) {
  auto pwallet = pwalletMain.get();
  auto &wallet_ext = pwallet->GetWalletExtension();

  CKey spending_key;
  spending_key.MakeNewKey(/* compressed: */ true);
  CPubKey spending_pubkey = spending_key.GetPubKey();

  CKey staking_key;
  staking_key.MakeNewKey(true);
  CPubKey staking_pubkey = staking_key.GetPubKey();

  LOCK2(cs_main, pwalletMain->cs_wallet);

  // Regular transactions don't affect remote staking balance
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateP2PKHScript(ToByteVector(spending_pubkey.GetID())));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    pwalletMain->LoadToWallet(wtx);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...neither do other people's remote staking transactions...
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateRemoteStakingScript(
      ToByteVector(staking_pubkey.GetID()),
      ToByteVector(staking_pubkey.GetSha256())
    ));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    pwalletMain->LoadToWallet(wtx);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...we have to own the spending key for it to count.
  pwalletMain->AddKey(spending_key);
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateRemoteStakingScript(
      ToByteVector(staking_pubkey.GetID()),
      ToByteVector(spending_pubkey.GetSha256())
    ));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    pwalletMain->LoadToWallet(wtx);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 100);
  }
}

BOOST_AUTO_TEST_SUITE_END()
