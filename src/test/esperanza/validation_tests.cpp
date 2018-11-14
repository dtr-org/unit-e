#include <esperanza/finalizationstate.h>
#include <esperanza/validation.h>
#include <keystore.h>
#include <random.h>
#include <script/script.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using namespace esperanza;

const CAmount MIN_DEPOSIT_SIZE = 100000 * UNIT;
const int64_t EPOCH_LENGTH = 50;

BOOST_FIXTURE_TEST_SUITE(finalization_validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(isvoteexpired) {

  FinalizationState *esperanza = FinalizationState::GetState();

  CKey k;
  InsecureNewKey(k, true);
  uint160 validatorAddress = k.GetPubKey().GetID();

  BOOST_CHECK_EQUAL(
      esperanza->ValidateDeposit(validatorAddress, MIN_DEPOSIT_SIZE),
      +Result::SUCCESS);

  esperanza->ProcessDeposit(validatorAddress, MIN_DEPOSIT_SIZE);

  // Initialize few epoch - since epoch 4 we don't have instant finalization
  for (int i = 1; i < 6; i++) {
    BOOST_CHECK_EQUAL(esperanza->InitializeEpoch(i * EPOCH_LENGTH),
                      +Result::SUCCESS);
  }

  uint256 targetHash = uint256();

  Vote expired{RandValidatorAddr(), targetHash, 0, 2};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(expired, k)), true);

  Vote current{RandValidatorAddr(), targetHash, 0, 6};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(current, k)), false);

  Vote afterLastFinalization{RandValidatorAddr(), targetHash, 0, 4};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(afterLastFinalization, k)), false);

  Vote future{RandValidatorAddr(), targetHash, 0, 12};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(future, k)), false);

  Vote currentOtherFork{RandValidatorAddr(), GetRandHash(), 0, 6};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(currentOtherFork, k)), false);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_deposit) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction deposit = CreateDepositTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(deposit, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_logout) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction logout = CreateLogoutTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(logout, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_withdraw) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::LOGOUT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction withdraw = CreateWithdrawTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(withdraw, validatorAddress));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validatorAddress.GetHex());
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_p2pkh_fails) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::STANDARD);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction p2pkh = CreateP2PKHTx(prevTx, k, 10000);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validatorAddress) == false);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_vote_fails) {

  Vote vote{};

  CKey k;
  InsecureNewKey(k, true);

  CTransaction p2pkh = CreateVoteTx(vote, k);
  uint160 validatorAddress = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validatorAddress) == false);
}

BOOST_AUTO_TEST_SUITE_END()
