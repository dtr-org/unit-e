// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>
#include <blockchain/blockchain_behavior.h>
#include <chainparams.h>
#include <consensus/validation.h>
#include <esperanza/vote.h>
#include <esperanza/walletextension.h>
#include <key.h>
#include <key_io.h>
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

namespace {

class FinalizationRewardLogicStub : public proposer::FinalizationRewardLogic {
 public:
  std::vector<std::pair<CScript, CAmount>> GetFinalizationRewards(const CBlockIndex &blockIndex) const override {
    return {};
  }
  size_t GetNumberOfRewardOutputs(blockchain::Height height) const override {
    return 0;
  }
};

}  // namespace

BOOST_AUTO_TEST_SUITE(walletextension_tests)

BOOST_FIXTURE_TEST_CASE(vote_signature, ReducedTestingSetup) {

  CBasicKeyStore keystore;
  CKey k;
  InsecureNewKey(k, true);
  keystore.AddKey(k);

  CPubKey pk = k.GetPubKey();

  esperanza::Vote vote{pk.GetID(), GetRandHash(), 10, 100};
  std::vector<unsigned char> voteSig;
  BOOST_CHECK(CreateVoteSignature(&keystore, vote, voteSig));

  std::vector<unsigned char> expectedSig;
  k.Sign(vote.GetHash(), expectedSig);

  BOOST_CHECK_EQUAL(HexStr(expectedSig), HexStr(voteSig));
  BOOST_CHECK(CheckVoteSignature(pk, vote, voteSig));
}

BOOST_FIXTURE_TEST_CASE(vote_signature2, TestingSetup) {

  CBasicKeyStore keystore;

  auto params = CreateChainParams(CBaseChainParams::TESTNET);

  CTxDestination dest = DecodeDestination("muUx4dQ4bwssNQYpUqAJHSJCUonAZ4Ro2s");
  const CKeyID *keyID = boost::get<CKeyID>(&dest);

  esperanza::Vote vote{*keyID, uint256S("4e7eae1625c033a05e92cff8d1591e4c7511888c264dbc8917ef94c3e66f22ef"), 12, 13};

  std::string pkey = "cNJWVLVrfrxZT85cwYfHdbRKGi2FQjkKFBjocwwinNNix5tytG33";

  CKey key = DecodeSecret(pkey);
  keystore.AddKey(key);

  std::vector<unsigned char> voteSig;
  BOOST_CHECK(CreateVoteSignature(&keystore, vote, voteSig));
}

BOOST_FIXTURE_TEST_CASE(sign_coinbase_transaction, WalletTestingSetup) {

  const key::mnemonic::Seed seed("stizzoso atavico inodore srotolato birra stupendo velina incendio copione pietra alzare privato folata madama gemmato");
  const CExtKey &ext_key = seed.GetExtKey();

  const auto key = ext_key.key;
  const auto pubkey = key.GetPubKey();
  const auto pubkeydata = std::vector<unsigned char>(pubkey.begin(), pubkey.end());

  auto behavior = blockchain::Behavior::NewFromParameters(blockchain::Parameters::TestNet());
  auto active_chain = staking::ActiveChain::New();
  FinalizationRewardLogicStub finalization_reward_logic;
  auto block_builder = proposer::BlockBuilder::New(&settings, &finalization_reward_logic);

  {
    LOCK(m_wallet->cs_wallet);
    m_wallet->AddKeyPubKey(key, pubkey);
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
    LOCK(m_wallet->cs_wallet);
    const CWallet *wallet = m_wallet.get();

    CWalletTx walletTx1(wallet, tx1ref);
    CWalletTx walletTx2(wallet, tx2ref);
    CWalletTx walletTx3(wallet, tx3ref);

    m_wallet->LoadToWallet(walletTx1);
    m_wallet->LoadToWallet(walletTx2);
    m_wallet->LoadToWallet(walletTx3);
  }

  const CBlockIndex block = [] {
    CBlockIndex index;
    index.nHeight = 230;
    return index;
  }();

  CScript prev_script_pubkey = CScript::CreateP2PKHScript(ToByteVector(pubkey.GetID()));
  staking::Coin coin1(&block, {tx1ref->GetHash(), 0}, {100, prev_script_pubkey});
  staking::Coin coin2(&block, {tx2ref->GetHash(), 0}, {1250, prev_script_pubkey});
  staking::Coin coin3(&block, {tx3ref->GetHash(), 0}, {125, prev_script_pubkey});
  proposer::EligibleCoin eligible_coin{
      coin2,       // coin used as stake
      uint256(),   // kernel hash
      5000,        // reward
      7251,        // target height
      1548255362,  // target time,
      0x1d00ffff   // difficulty = 1
  };

  staking::CoinSet coins;
  coins.insert(coin1);
  coins.insert(coin2);
  coins.insert(coin3);

  // BuildCoinbaseTransaction() will also sign it
  CTransactionRef coinbase_transaction =
      block_builder->BuildCoinbaseTransaction(*active_chain->GetTip(), uint256(), eligible_coin, coins, 700, boost::none, m_wallet->GetWalletExtension());

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
    BOOST_CHECK(::IsMine(*m_wallet, out.scriptPubKey) == ISMINE_SPENDABLE);
  }
}

BOOST_FIXTURE_TEST_CASE(get_remote_staking_balance, WalletTestingSetup) {
  auto pwallet = m_wallet.get();
  auto &wallet_ext = pwallet->GetWalletExtension();

  CKey our_key;
  our_key.MakeNewKey(/* compressed: */ true);
  CPubKey our_pubkey = our_key.GetPubKey();

  CKey their_key;
  their_key.MakeNewKey(true);
  CPubKey their_pubkey = their_key.GetPubKey();

  CKey random_key;
  random_key.MakeNewKey(true);
  CPubKey random_pubkey = random_key.GetPubKey();

  const auto our_script = CScript() << ToByteVector(our_pubkey) << OP_CHECKSIG;
  const auto our_script_hash = Sha256(our_script.begin(), our_script.end());

  const auto their_script = CScript() << ToByteVector(their_pubkey) << OP_CHECKSIG;
  const auto their_script_hash = Sha256(their_script.begin(), their_script.end());

  LOCK2(cs_main, m_wallet->cs_wallet);
  pwallet->AddKey(our_key);
  pwallet->AddCScript(our_script);

  // P2PKH transactions don't affect remote staking balance
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateP2PKHScript(ToByteVector(our_pubkey.GetID())));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    m_wallet->LoadToWallet(wtx);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...neither do P2PK transactions
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript() << ToByteVector(our_pubkey) << OP_CHECKSIG);
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    m_wallet->LoadToWallet(wtx);
    BOOST_CHECK_EQUAL(m_wallet->IsMine(tx.vout[0]), ISMINE_SPENDABLE);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...neither do other people's remote staking transactions...
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateRemoteStakingKeyhashScript(
                                  ToByteVector(their_pubkey.GetID()),
                                  ToByteVector(random_pubkey.GetSha256())));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    m_wallet->LoadToWallet(wtx);

    CMutableTransaction tx2;
    tx2.vout.emplace_back(100, CScript::CreateRemoteStakingScripthashScript(
                                   ToByteVector(their_pubkey.GetID()),
                                   ToByteVector(their_script_hash)));
    CWalletTx wtx2(pwallet, MakeTransactionRef(tx2));
    m_wallet->LoadToWallet(wtx2);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...or tranctions that other people are staking on this node
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateRemoteStakingKeyhashScript(
                                  ToByteVector(our_pubkey.GetID()),
                                  ToByteVector(their_pubkey.GetSha256())));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    m_wallet->LoadToWallet(wtx);

    CMutableTransaction tx2;
    tx2.vout.emplace_back(100, CScript::CreateRemoteStakingScripthashScript(
                                   ToByteVector(our_pubkey.GetID()),
                                   ToByteVector(their_script_hash)));
    CWalletTx wtx2(pwallet, MakeTransactionRef(tx2));
    m_wallet->LoadToWallet(wtx2);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 0);
  }

  // ...we have to be able to spend it to count.
  {
    CMutableTransaction tx;
    tx.vout.emplace_back(100, CScript::CreateRemoteStakingKeyhashScript(
                                  ToByteVector(their_pubkey.GetID()),
                                  ToByteVector(our_pubkey.GetSha256())));
    CWalletTx wtx(pwallet, MakeTransactionRef(tx));
    m_wallet->LoadToWallet(wtx);

    CMutableTransaction tx2;
    tx2.vout.emplace_back(100, CScript::CreateRemoteStakingScripthashScript(
                                   ToByteVector(their_pubkey.GetID()),
                                   ToByteVector(our_script_hash)));
    CWalletTx wtx2(pwallet, MakeTransactionRef(tx2));
    m_wallet->LoadToWallet(wtx2);

    CAmount balance = wallet_ext.GetRemoteStakingBalance();
    BOOST_CHECK_EQUAL(balance, 200);
  }
}

BOOST_FIXTURE_TEST_CASE(get_stakeable_coins, TestChain100Setup) {

  const auto pwallet = m_wallet.get();
  const auto &wallet_ext = pwallet->GetWalletExtension();

  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    BOOST_CHECK_EQUAL(wallet_ext.GetStakeableCoins().size(), 1);
  }

  // Make the first coinbase mature
  CScript coinbase_script = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
  bool processed;
  CreateAndProcessBlock({}, coinbase_script, &processed);
  BOOST_CHECK(processed);

  CTransaction &stakeable = m_coinbase_txns.front();

  // Check that a coin can be selected
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    staking::CoinSet stakeable_coins = wallet_ext.GetStakeableCoins();
    BOOST_REQUIRE_EQUAL(stakeable_coins.size(), 2);  // The just created stakeable tx + initial reward

    bool found = false;
    for (const staking::Coin &coin : stakeable_coins) {
      if (stakeable.GetHash() == coin.GetTransactionId() && coin.GetOutputIndex() == 0) {
        found = true;
        break;
      }
    }
    BOOST_CHECK(found);
  }

  // Make sure locked coins are not selected
  {
    LOCK2(cs_main, m_wallet->cs_wallet);
    staking::CoinSet stakeable_coins = wallet_ext.GetStakeableCoins();
    BOOST_CHECK_EQUAL(stakeable_coins.size(), 2);  // The just created stakeable tx + initial reward

    pwallet->LockCoin(COutPoint(stakeable.GetHash(), 0));

    stakeable_coins = wallet_ext.GetStakeableCoins();
    BOOST_CHECK_EQUAL(stakeable_coins.size(), 1);

    // Make sure we select the other coin in the coinbase
    BOOST_CHECK(stakeable_coins.begin()->GetTransactionId() != stakeable.GetHash());
    BOOST_CHECK(stakeable_coins.begin()->GetOutputIndex() != 0);
  }
}

BOOST_AUTO_TEST_SUITE_END()
