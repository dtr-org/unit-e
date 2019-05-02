// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_WALLET_TEST_WALLET_TEST_FIXTURE_H
#define UNITE_WALLET_TEST_WALLET_TEST_FIXTURE_H

#include <test/test_unite.h>
#include <test/test_unite_mocks.h>

#include <wallet/wallet.h>

#include <memory>

//! Testing setup and teardown for wallet.
struct WalletTestingSetup : public TestingSetup {
  explicit WalletTestingSetup(
      const std::string &chainName = CBaseChainParams::TESTNET,
      UnitEInjectorConfiguration config = UnitEInjectorConfiguration());

  explicit WalletTestingSetup(
      std::function<void(Settings&)> f,
      const std::string &chainName = CBaseChainParams::TESTNET,
      UnitEInjectorConfiguration config = UnitEInjectorConfiguration());

  ~WalletTestingSetup();

  Settings settings;
  std::shared_ptr<CWallet> m_wallet;
  mocks::StakeValidatorMock stake_validator_mock;
};

//
// Testing fixture that pre-creates a
// 100-block REGTEST-mode block chain
//
struct TestChain100Setup : public WalletTestingSetup {
  TestChain100Setup(UnitEInjectorConfiguration config = UnitEInjectorConfiguration());

  // Create a new block with just given transactions, coinbase paying to
  // scriptPubKey, and try to add it to the current chain.
  //
  // Asserts that the a new block was successfully created. Alternatively
  // a pointer to a bool can be passed in which the result will be stored in.
  CBlock CreateAndProcessBlock(const std::vector<CMutableTransaction>& txns,
                               const CScript& scriptPubKey,
                               bool *processed = nullptr);

  ~TestChain100Setup();

  std::vector<CTransaction> m_coinbase_txns; // For convenience, coinbase transactions
  CKey coinbaseKey; // private/public key needed to spend coinbase transactions
};

#endif // UNITE_WALLET_TEST_WALLET_TEST_FIXTURE_H
