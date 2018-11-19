// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpcvalidator.h>

#include <test/rpc_test_utils.h>
#include <test/test_unite.h>
#include <wallet/test/wallet_test_fixture.h>
#include <boost/test/unit_test.hpp>

struct ValidatorWalletSetup : WalletTestingSetup {
  ValidatorWalletSetup() : WalletTestingSetup(true){};
};

BOOST_AUTO_TEST_SUITE(rpcvalidator_tests)

CTxDestination GetDestination(CWallet &wallet, OutputType type) {
  CPubKey pk;
  {
    LOCK(wallet.cs_wallet);
    CKey k;
    InsecureNewKey(k, true);
    wallet.AddKey(k);

    pk = k.GetPubKey();
  }
  return GetDestinationForKey(pk, type);
}

//Test deposit

BOOST_FIXTURE_TEST_CASE(deposit_p2sh_segwit_not_supported, ValidatorWalletSetup) {

  CTxDestination p2sh = GetDestination(*pwalletMain.get(), OUTPUT_TYPE_P2SH_SEGWIT);

  std::string command = "deposit " + EncodeDestination(p2sh) + " 1500";
  AssertRPCError(command, RPC_INVALID_ADDRESS_OR_KEY, "Address must be a P2PKH address.");
}

BOOST_FIXTURE_TEST_CASE(deposit_bech32_not_supported, ValidatorWalletSetup) {

  CTxDestination bech32 = GetDestination(*pwalletMain.get(), OUTPUT_TYPE_BECH32);

  std::string command = "deposit " + EncodeDestination(bech32) + " 1500";
  AssertRPCError(command, RPC_INVALID_ADDRESS_OR_KEY, "Address must be a P2PKH address.");
}

BOOST_FIXTURE_TEST_CASE(deposit_p2pkh_supported_but_not_enough_funds, ValidatorWalletSetup) {

  CTxDestination p2pkh = GetDestination(*pwalletMain.get(), OUTPUT_TYPE_LEGACY);

  std::string command = "deposit " + EncodeDestination(p2pkh) + " 1499";
  AssertRPCError(command, RPC_INVALID_PARAMETER, "Amount is below minimum allowed.");
}

BOOST_FIXTURE_TEST_CASE(deposit_not_a_validator, WalletTestingSetup) {

  CTxDestination p2pkh = GetDestination(*pwalletMain.get(), OUTPUT_TYPE_LEGACY);

  std::string command = "deposit " + EncodeDestination(p2pkh) + " 0";
  AssertRPCError(command, RPC_INVALID_PARAMETER, "Validating is disabled.");
}

BOOST_AUTO_TEST_SUITE_END()
