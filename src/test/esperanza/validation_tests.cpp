#include <esperanza/finalizationstate.h>
#include <esperanza/validation.h>
#include <keystore.h>
#include <random.h>
#include <script/script.h>
#include <test/esperanza/finalization_utils.h>
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
  assert(esperanza != nullptr);

  uint256 validatorIndex = GetRandHash();

  BOOST_CHECK_EQUAL(
      esperanza->ValidateDeposit(validatorIndex, MIN_DEPOSIT_SIZE),
      +Result::SUCCESS);

  esperanza->ProcessDeposit(validatorIndex, MIN_DEPOSIT_SIZE);

  // Initialize few epoch - since epoch 4 we don't have instant finalization
  for (int i = 1; i < 6; i++) {
    BOOST_CHECK_EQUAL(esperanza->InitializeEpoch(i * EPOCH_LENGTH),
                      +Result::SUCCESS);
  }

  uint256 targetHash = uint256();

  Vote expired{GetRandHash(), targetHash, 0, 2};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(expired)), true);

  Vote current{GetRandHash(), targetHash, 0, 6};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(current)), false);

  Vote afterLastFinalization{GetRandHash(), targetHash, 0, 4};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(afterLastFinalization)), false);

  Vote future{GetRandHash(), targetHash, 0, 12};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(future)), false);

  Vote currentOtherFork{GetRandHash(), GetRandHash(), 0, 6};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(currentOtherFork)), false);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_deposit) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction deposit = CreateDepositTx(prevTx, k, 10000);
  uint256 validatorIndex = uint256();
  BOOST_CHECK(ExtractValidatorIndex(deposit, validatorIndex));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetHash(), validatorIndex);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_logout) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction logout = CreateLogoutTx(prevTx, k, 10000);
  uint256 validatorIndex = uint256();
  BOOST_CHECK(ExtractValidatorIndex(logout, validatorIndex));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetHash(), validatorIndex);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_withdraw) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::LOGOUT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction withdraw = CreateWithdrawTx(prevTx, k, 10000);
  uint256 validatorIndex = uint256();
  BOOST_CHECK(ExtractValidatorIndex(withdraw, validatorIndex));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetHash(), validatorIndex);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_p2pkh_fails) {

  SeedInsecureRand();
  CBasicKeyStore keystore;

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::STANDARD);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prevTx(tx);

  CTransaction p2pkh = CreateP2PKHTx(prevTx, k, 10000);
  uint256 validatorIndex = uint256();
  BOOST_CHECK(ExtractValidatorIndex(p2pkh, validatorIndex) == false);
}

BOOST_AUTO_TEST_CASE(extractvalidatorindex_vote_fails) {

  Vote vote{};

  CTransaction p2pkh = CreateVoteTx(vote);
  uint256 validatorIndex = uint256();
  BOOST_CHECK(ExtractValidatorIndex(p2pkh, validatorIndex) == false);
}

BOOST_AUTO_TEST_SUITE_END()
