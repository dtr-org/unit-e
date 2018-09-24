#include <esperanza/finalizationstate.h>
#include <esperanza/validation.h>
#include <random.h>
#include <script/script.h>
#include <test/test_unite.h>
#include <util.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using namespace esperanza;

CTransaction CreateVoteTx(Vote &vote) {
  CMutableTransaction mutTx;
  mutTx.SetType(TxType::VOTE);

  mutTx.vin.resize(1);
  uint256 signature = GetRandHash();

  CScript encodedVote = CScript::EncodeVote(vote);
  std::vector<unsigned char> voteVector(encodedVote.begin(), encodedVote.end());

  CScript voteScript = (CScript() << ToByteVector(signature)) << voteVector;
  mutTx.vin[0] = (CTxIn(GetRandHash(), 0, voteScript));

  CTxOut out{10000, CScript()};
  mutTx.vout.push_back(out);

  return CTransaction{mutTx};
}

const CAmount MIN_DEPOSIT_SIZE = 100000 * UNIT;
const int64_t EPOCH_LENGTH = 5;

BOOST_FIXTURE_TEST_SUITE(esperanza_validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(isvoteexpired) {

  FinalizationState *esperanza = FinalizationState::GetState();

  uint256 validatorIndex = GetRandHash();

  BOOST_CHECK(esperanza->ValidateDeposit(validatorIndex, MIN_DEPOSIT_SIZE) ==
              +Result::SUCCESS);
  esperanza->ProcessDeposit(validatorIndex, MIN_DEPOSIT_SIZE);

  // Initialize few epoch - since epoch 4 we don't have instant finalization
  for (int i = 1; i < 6; i++) {
    BOOST_CHECK(esperanza->InitializeEpoch(i * EPOCH_LENGTH) ==
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

BOOST_AUTO_TEST_SUITE_END()
