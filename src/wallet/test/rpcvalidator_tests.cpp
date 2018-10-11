#include <wallet/rpcvalidator.h>

#include <test/rpc_test_utils.h>
#include <test/test_unite.h>
#include <wallet/test/wallet_test_fixture.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(rpcvalidator_tests, WalletTestingSetup)

CWallet &SetupWallet(CWallet *mainWallet) {
  CWallet &wallet = *mainWallet;
  wallet.GetWalletExtension().nIsValidatorEnabled = true;
  return wallet;
}

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

BOOST_AUTO_TEST_CASE(deposit_p2sh_segwit_not_supported) {

  CWallet &wallet = SetupWallet(pwalletMain.get());
  CTxDestination p2sh = GetDestination(wallet, OUTPUT_TYPE_P2SH_SEGWIT);

  std::string command = "deposit " + EncodeDestination(p2sh) + " 1000000";
  AssertRPCError(command, RPC_INVALID_ADDRESS_OR_KEY);
}

BOOST_AUTO_TEST_CASE(deposit_bech32_not_supported) {

  CWallet &wallet = SetupWallet(pwalletMain.get());
  CTxDestination bech32 = GetDestination(wallet, OUTPUT_TYPE_BECH32);

  std::string command = "deposit " + EncodeDestination(bech32) + " 1000000";
  AssertRPCError(command, RPC_INVALID_ADDRESS_OR_KEY);
}

BOOST_AUTO_TEST_CASE(deposit_p2pkh_supported_but_not_enough_funds) {

  CWallet &wallet = SetupWallet(pwalletMain.get());
  CTxDestination p2pkh = GetDestination(wallet, OUTPUT_TYPE_LEGACY);

  std::string command = "deposit " + EncodeDestination(p2pkh) + " 1000000";
  //Fails creating the transaction since there aren't enough funds
  AssertRPCError(command, RPC_TRANSACTION_ERROR);
}

BOOST_AUTO_TEST_SUITE_END()
