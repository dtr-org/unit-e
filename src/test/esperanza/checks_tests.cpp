// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/checks.h>
#include <esperanza/finalizationstate.h>
#include <keystore.h>
#include <random.h>
#include <script/script.h>
#include <test/esperanza/finalization_utils.h>
#include <test/esperanza/finalizationstate_utils.h>
#include <test/test_unite.h>
#include <util.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>

using namespace esperanza;

BOOST_FIXTURE_TEST_SUITE(finalization_checks_tests, TestingSetup)

CTransaction CreateAdminTx(const AdminKeySet &key_set) {

  CMutableTransaction mtx;
  mtx.SetType(TxType::ADMIN);
  mtx.vin.resize(1);
  mtx.vout.resize(1);

  AdminCommand cmd(AdminCommandType::END_PERMISSIONING, {});
  CScript script = EncodeAdminCommand(cmd);
  mtx.vout = {CTxOut(1, script)};

  CScript witness_script = CScript() << 1 << ToByteVector(key_set[0]) << ToByteVector(key_set[1])
                                     << ToByteVector(key_set[2]) << 3 << OP_CHECKMULTISIG;
  CTxIn input(GetRandHash(), 0);
  input.scriptWitness.stack.resize(3);
  input.scriptWitness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
  mtx.vin = {input};

  return CTransaction(mtx);
}

CTransaction CreateSlashTx(const CPubKey &pub_key, const Vote &vote1, const Vote &vote2) {

  CScript vout_script = CScript::CreatePayVoteSlashScript(pub_key);

  CMutableTransaction mtx;
  mtx.SetType(TxType::SLASH);
  mtx.vin.resize(1);
  mtx.vout.resize(1);
  mtx.vout = {CTxOut(1, vout_script)};

  CScript encoded_vote1 = CScript::EncodeVote(vote1, ToByteVector(GetRandHash()));
  std::vector<unsigned char> vote1_vector(encoded_vote1.begin(), encoded_vote1.end());

  CScript encoded_vote2 = CScript::EncodeVote(vote2, ToByteVector(GetRandHash()));
  std::vector<unsigned char> vote2_vector(encoded_vote2.begin(), encoded_vote2.end());

  CScript vinScript = CScript() << ToByteVector(GetRandHash()) << vote1_vector << vote2_vector;

  mtx.vin = {CTxIn(GetRandHash(), 0, vinScript)};

  return CTransaction(mtx);
}

FinalizationParams CreateFinalizationParams() {
  FinalizationParams params;

  params.epoch_length = 10;
  params.min_deposit_size = 10;
  params.withdrawal_epoch_delay = 0;
  params.bounty_fraction_denominator = 2;
  params.base_interest_factor = 700000000;

  return params;
}

BOOST_AUTO_TEST_CASE(CheckAdminTx_test) {
  AdminCommand cmd(AdminCommandType::END_PERMISSIONING, {});
  CScript valid_script = EncodeAdminCommand(cmd);
  CScript invalid_script(valid_script.begin(), valid_script.begin() + 1);

  CMutableTransaction mtx;
  mtx.SetType(TxType::ADMIN);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK_EQUAL("admin-vin-empty", err_state.GetRejectReason());
  }

  mtx.vin.resize(1);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK_EQUAL("admin-vout-empty", err_state.GetRejectReason());
  }

  mtx.vout.resize(2);

  {
    // Check for admin commands
    CTransaction tx(mtx);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK_EQUAL("admin-no-commands", err_state.GetRejectReason());
  }

  mtx.vout = {CTxOut(1, invalid_script)};

  {
    // Validate admin command
    CTransaction tx(mtx);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK_EQUAL("admin-invalid-command", err_state.GetRejectReason());
  }

  mtx.vout = {CTxOut(1, valid_script), CTxOut(1, valid_script)};

  {
    // Check double permissioning
    CTransaction tx(mtx);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK_EQUAL("admin-double-disable", err_state.GetRejectReason());
  }

  {
    AdminKeySet key_set = MakeKeySet();
    CTransaction tx = CreateAdminTx(key_set);
    CValidationState err_state;
    std::vector<CPubKey> keys_out;

    CheckAdminTx(tx, err_state, &keys_out);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckAdminTx_test) {
  {
    // Check admin's permissioning
    CTransaction tx = CreateAdminTx(MakeKeySet());

    FinalizationStateSpy spy(FinalizationParams{}, AdminParams{});
    CValidationState err_state;

    bool ok = ContextualCheckAdminTx(tx, err_state, spy);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "admin-disabled");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 10);
  }

  {
    AdminKeySet key_set = MakeKeySet();
    CTransaction tx = CreateAdminTx(key_set);

    AdminParams admin_params;
    admin_params.admin_keys = key_set;

    FinalizationStateSpy spy(FinalizationParams{}, admin_params);
    CValidationState err_state;

    bool ok = ContextualCheckAdminTx(tx, err_state, spy);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(CheckDepositTx_test) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::DEPOSIT);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckDepositTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vin.resize(1);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckDepositTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vout.resize(1);

  {
    // Validate vout script type
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckDepositTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-vout-script");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CTransaction tx = CreateDepositTx(CTransaction(mtx), key, 1);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckDepositTx(tx, err_state, &va_out);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckDepositTx_test) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::DEPOSIT);
  mtx.vin.resize(1);
  mtx.vout.resize(1);

  {
    // insufficient amount in deposit
    CTransaction deposit = CreateDepositTx(CTransaction(mtx), key, 10000 * UNIT, 9000 * UNIT);
    CValidationState err_state;

    FinalizationState fin_state(FinalizationParams{}, AdminParams{});
    bool ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-invalid");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    // duplicate deposit
    CTransaction deposit = CreateDepositTx(CTransaction(mtx), key, 10000 * UNIT);
    CValidationState err_state;
    FinalizationState fin_state(FinalizationParams{}, AdminParams{});

    bool ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);

    uint160 address;
    BOOST_CHECK(ExtractValidatorAddress(deposit, address));
    fin_state.ProcessDeposit(address, deposit.vout[0].nValue);

    ok = ContextualCheckDepositTx(deposit, err_state, fin_state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-deposit-duplicate");
    BOOST_CHECK(!err_state.IsValid());

    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }
}

BOOST_AUTO_TEST_CASE(CheckLogoutTx_test) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::LOGOUT);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckLogoutTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-logout-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vin.resize(1);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckLogoutTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-logout-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vout.resize(1);

  {
    // Validate vout script type
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckLogoutTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-logout-vout-script");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CTransaction tx = CreateLogoutTx(CTransaction(mtx), key, 1);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckLogoutTx(tx, err_state, &va_out);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckLogoutTx_test) {
  CKey key;
  InsecureNewKey(key, true);
  CPubKey pkey = key.GetPubKey();
  uint160 validator_address = pkey.GetID();

  CScript script = CScript::CreatePayVoteSlashScript(pkey);

  CMutableTransaction mtx;
  mtx.SetType(TxType::DEPOSIT);
  mtx.vin.resize(1);
  mtx.vout.resize(1);
  mtx.vout = {CTxOut(1, script)};
  CTransactionRef prev_tx = MakeTransactionRef(mtx);

  CCoinsView view;

  {
    CTransaction tx = CreateLogoutTx(*prev_tx, key, 1);
    CValidationState err_state;
    FinalizationStateSpy spy(FinalizationParams{}, AdminParams{});

    bool ok = ContextualCheckLogoutTx(tx, err_state, spy, view);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-logout-not-from-validator");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CTransaction tx = CreateLogoutTx(*prev_tx, key, 1);
    CValidationState err_state;

    FinalizationStateSpy spy;
    CAmount deposit_size = spy.MinDepositSize();

    uint256 target_hash = GetRandHash();
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    bool ok = ContextualCheckLogoutTx(tx, err_state, spy, view);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-logout-no-prev-tx-found");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }

  {
    CTransaction tx = CreateLogoutTx(*prev_tx, key, 10000);

    TestMemPoolEntryHelper entry;
    mempool.addUnchecked(prev_tx->GetHash(),
                         entry.Fee(1000).Time(GetTime()).SpendsCoinbase(true).FromTx(prev_tx));

    CValidationState err_state;

    FinalizationStateSpy spy;
    CAmount deposit_size = spy.MinDepositSize();

    uint256 target_hash = GetRandHash();
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    bool ok = ContextualCheckLogoutTx(tx, err_state, spy, view);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(CheckSlashTx_test) {
  CKey key;
  InsecureNewKey(key, true);
  CPubKey pub_key = key.GetPubKey();

  CMutableTransaction mtx;
  mtx.SetType(TxType::SLASH);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote1;
    Vote vote2;

    bool ok = CheckSlashTx(tx, err_state, &vote1, &vote2);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vin.resize(1);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote1;
    Vote vote2;

    bool ok = CheckSlashTx(tx, err_state, &vote1, &vote2);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vout.resize(1);

  {
    // Check slash data format
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote1;
    Vote vote2;

    bool ok = CheckSlashTx(tx, err_state, &vote1, &vote2);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-data-format");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    Vote vote1;
    Vote vote2;

    CTransaction tx = CreateSlashTx(pub_key, vote1, vote2);
    CValidationState err_state;

    bool ok = CheckSlashTx(tx, err_state, &vote1, &vote2);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckSlashTx_test) {
  CKey key;
  InsecureNewKey(key, true);
  CPubKey pub_key = key.GetPubKey();
  uint160 validator_address = pub_key.GetID();

  {
    Vote vote1;
    Vote vote2;

    CTransaction tx = CreateSlashTx(pub_key, vote1, vote2);
    CValidationState err_state;
    FinalizationState fin_state(FinalizationParams{}, AdminParams{});

    bool ok = ContextualCheckSlashTx(tx, err_state, fin_state);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-not-slashable");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    Vote vote1{validator_address, GetRandHash(), 10, 100};
    Vote vote2{validator_address, GetRandHash(), 10, 100};

    CTransaction tx = CreateSlashTx(pub_key, vote1, vote2);
    CValidationState err_state;
    FinalizationStateSpy spy;

    CAmount deposit_size = spy.MinDepositSize();

    uint256 target_hash = GetRandHash();
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    spy.SetRecommendedTarget(block_index);

    BOOST_CHECK_EQUAL(spy.ValidateDeposit(validator_address, deposit_size),
                      +Result::SUCCESS);
    spy.ProcessDeposit(validator_address, deposit_size);

    // Slash too early
    bool ok = ContextualCheckSlashTx(tx, err_state, spy);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-not-slashable");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);

    for (uint32_t i = 1; i < 6 * spy.EpochLength() + 1; i += spy.EpochLength()) {
      Result res = spy.InitializeEpoch(i);
      BOOST_CHECK_EQUAL(res, +Result::SUCCESS);
    }
    BOOST_CHECK_EQUAL(spy.GetCurrentEpoch(), 6);

    err_state = CValidationState();
    ok = ContextualCheckSlashTx(tx, err_state, spy);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());

    spy.ProcessSlash(vote1, vote2);

    // Duplicate slash
    err_state = CValidationState();
    ok = ContextualCheckSlashTx(tx, err_state, spy);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-slash-not-slashable");

    dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }
}

BOOST_AUTO_TEST_CASE(CheckVoteTx_test) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::VOTE);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote_out;
    std::vector<unsigned char> vote_sig_out;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vin.resize(1);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote_out;
    std::vector<unsigned char> vote_sig_out;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vout.resize(1);

  {
    // Check vote vout script
    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote_out;
    std::vector<unsigned char> vote_sig_out;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-vout-script");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CScript script = CScript::CreatePayVoteSlashScript(key.GetPubKey());
    mtx.vout = {CTxOut(1, script)};

    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote_out;
    std::vector<unsigned char> vote_sig_out;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-data-format");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    Vote vote;

    CScript encoded_vote = CScript::EncodeVote(vote, ToByteVector(GetRandHash()));
    std::vector<unsigned char> voteVector(encoded_vote.begin(), encoded_vote.end());

    CScript script = (CScript() << ToByteVector(GetRandHash())) << voteVector;
    mtx.vin = {CTxIn(GetRandHash(), 0, script)};

    CTransaction tx(mtx);
    CValidationState err_state;
    Vote vote_out;
    std::vector<unsigned char> vote_sig_out;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-signature");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CTransaction prev_tx;

    CBasicKeyStore keystore;
    CKey key;
    InsecureNewKey(key, true);
    keystore.AddKey(key);
    CPubKey pub_key = key.GetPubKey();

    Vote vote_out{pub_key.GetID(), GetRandHash(), 10, 100};

    std::vector<unsigned char> vote_sig_out;
    BOOST_CHECK(Vote::CreateSignature(&keystore, vote_out, vote_sig_out));

    CTransaction tx = CreateVoteTx(prev_tx, key, vote_out, vote_sig_out);
    CValidationState err_state;

    bool ok = CheckVoteTx(tx, err_state, &vote_out, &vote_sig_out);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckVoteTx_test) {
  uint256 target_hash = GetRandHash();

  CBasicKeyStore keystore;
  CKey key;
  InsecureNewKey(key, true);
  keystore.AddKey(key);
  CPubKey pub_key = key.GetPubKey();
  uint160 validator_address = pub_key.GetID();

  Vote vote_out{pub_key.GetID(), target_hash, 0, 5};

  std::vector<unsigned char> vote_sig_out;
  BOOST_CHECK(Vote::CreateSignature(&keystore, vote_out, vote_sig_out));

  CMutableTransaction mt;
  mt.SetType(TxType::DEPOSIT);
  mt.vin.resize(1);
  mt.vout.resize(1);
  mt.vout = {CTxOut(1, CScript::CreatePayVoteSlashScript(pub_key))};
  CTransactionRef prev_tx = MakeTransactionRef(mt);

  CCoinsView view;

  {
    CTransaction tx = CreateVoteTx(*prev_tx, key, vote_out, vote_sig_out);
    CValidationState err_state;
    FinalizationStateSpy spy;

    CAmount deposit_size = spy.MinDepositSize();

    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    bool ok = ContextualCheckVoteTx(tx, err_state, spy, view);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-no-prev-tx-found");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }

  {
    TestMemPoolEntryHelper entry;
    mempool.addUnchecked(prev_tx->GetHash(), entry.Fee(1000).Time(GetTime()).SpendsCoinbase(true).FromTx(prev_tx));

    FinalizationStateSpy spy;
    CAmount deposit_size = spy.MinDepositSize();

    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    CTransaction tx = CreateVoteTx(*prev_tx, key, vote_out, vote_sig_out);
    CValidationState err_state;

    bool ok = ContextualCheckVoteTx(tx, err_state, spy, view);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(CheckWithdrawTx_test) {
  CKey key;
  InsecureNewKey(key, true);

  CMutableTransaction mtx;
  mtx.SetType(TxType::WITHDRAW);

  mtx.vout.resize(4);

  {
    // Check vout of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckWithdrawTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-withdraw-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vout.resize(1);

  {
    // Check vin of transaction
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckWithdrawTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-withdraw-malformed");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  mtx.vin.resize(1);

  {
    // Check P2PKH script
    CTransaction tx(mtx);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckWithdrawTx(tx, err_state, &va_out);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-withdraw-vout-script-invalid-p2pkh");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 100);
  }

  {
    CTransaction tx = CreateWithdrawTx(CTransaction(mtx), key, 1);
    CValidationState err_state;
    uint160 va_out;

    bool ok = CheckWithdrawTx(tx, err_state, &va_out);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(ContextualCheckWithdrawTx_test) {

  uint256 target_hash = GetRandHash();

  CKey key;
  InsecureNewKey(key, true);
  CPubKey pub_key = key.GetPubKey();
  uint160 validator_address = pub_key.GetID();

  CMutableTransaction mt;
  mt.SetType(TxType::LOGOUT);
  mt.vin.resize(1);
  mt.vout.resize(1);
  mt.vout = {CTxOut(1, CScript::CreatePayVoteSlashScript(pub_key))};
  CTransactionRef prev_tx = MakeTransactionRef(mt);

  CCoinsView view;

  {
    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;

    FinalizationStateSpy spy;
    CAmount deposit_size = spy.MinDepositSize();
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    CTransaction tx = CreateWithdrawTx(*prev_tx, key, 1);
    CValidationState err_state;

    bool ok = ContextualCheckWithdrawTx(tx, err_state, spy, view);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-withdraw-no-prev-tx-found");

    int dos = 0;
    err_state.IsInvalid(dos);
    BOOST_CHECK_EQUAL(dos, 0);
  }

  {
    TestMemPoolEntryHelper entry;
    mempool.addUnchecked(prev_tx->GetHash(), entry.Fee(1000).Time(GetTime()).SpendsCoinbase(true).FromTx(prev_tx));

    CBlockIndex block_index;
    block_index.phashBlock = &target_hash;

    FinalizationParams fin_params = CreateFinalizationParams();
    FinalizationStateSpy spy(fin_params, AdminParams{});
    CAmount deposit_size = spy.MinDepositSize();
    spy.SetRecommendedTarget(block_index);

    spy.CreateAndActivateDeposit(validator_address, deposit_size);

    BOOST_CHECK_EQUAL(spy.ValidateLogout(validator_address), +Result::SUCCESS);
    spy.ProcessLogout(validator_address);

    Validator *validator = &(*spy.pValidators())[validator_address];
    validator->m_end_dynasty = 0;

    CTransaction tx = CreateWithdrawTx(*prev_tx, key, 1);
    CValidationState err_state;

    bool ok = ContextualCheckWithdrawTx(tx, err_state, spy, view);
    BOOST_CHECK(ok);
    BOOST_CHECK(err_state.IsValid());
  }
}

BOOST_AUTO_TEST_CASE(IsVoteExpired_test) {

  FinalizationStateSpy spy;
  const auto &params = CreateChainParams(CBaseChainParams::TESTNET)->GetFinalization();
  const auto min_deposit = params.min_deposit_size;

  CKey k;
  InsecureNewKey(k, true);
  uint160 validator_address = k.GetPubKey().GetID();

  spy.CreateAndActivateDeposit(validator_address, min_deposit);

  uint256 target_hash = uint256();

  Vote expired{RandValidatorAddr(), target_hash, 0, 2};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(expired, k), spy), true);

  Vote current{RandValidatorAddr(), target_hash, 0, 5};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(current, k), spy), false);

  Vote afterLastFinalization{RandValidatorAddr(), target_hash, 0, 3};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(afterLastFinalization, k), spy), true);

  Vote future{RandValidatorAddr(), target_hash, 0, 12};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(future, k), spy), false);

  Vote currentOtherFork{RandValidatorAddr(), GetRandHash(), 0, 5};
  BOOST_CHECK_EQUAL(IsVoteExpired(CreateVoteTx(currentOtherFork, k), spy), false);
}

BOOST_AUTO_TEST_CASE(CheckVoteTransaction_malformed_vote) {

  CKey key;
  key.MakeNewKey(true);
  Vote vote = Vote{key.GetPubKey().GetID(), GetRandHash(), 0, 2};
  CTransaction tx = CreateVoteTx(vote, key);
  CMutableTransaction mutedTx(tx);
  FinalizationStateSpy spy;

  // Replace the vote with something meaningless
  mutedTx.vin[0].scriptSig = CScript() << 1337;

  CCoinsView view;

  CTransaction invalidVote(mutedTx);
  CValidationState err_state;

  bool ok = ContextualCheckVoteTx(invalidVote, err_state, spy, view);
  BOOST_CHECK(!ok);
  BOOST_CHECK_EQUAL(err_state.GetRejectReason(), "bad-vote-data-format");

  int dos = 0;
  err_state.IsInvalid(dos);
  BOOST_CHECK_EQUAL(dos, 100);
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_deposit) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prev_tx(tx);

  CTransaction deposit = CreateDepositTx(prev_tx, k, 10000);
  uint160 validator_address = uint160();
  BOOST_CHECK(ExtractValidatorAddress(deposit, validator_address));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validator_address.GetHex());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_logout) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::DEPOSIT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prev_tx(tx);

  CTransaction logout = CreateLogoutTx(prev_tx, k, 10000);
  uint160 validator_address = uint160();
  BOOST_CHECK(ExtractValidatorAddress(logout, validator_address));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validator_address.GetHex());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_withdraw) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::LOGOUT);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prev_tx(tx);

  CTransaction withdraw = CreateWithdrawTx(prev_tx, k, 10000);
  uint160 validator_address = uint160();
  BOOST_CHECK(ExtractValidatorAddress(withdraw, validator_address));

  BOOST_CHECK_EQUAL(k.GetPubKey().GetID().GetHex(), validator_address.GetHex());
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_p2pkh_fails) {

  CKey k;
  InsecureNewKey(k, true);

  CMutableTransaction tx;
  tx.SetType(TxType::REGULAR);
  tx.vin.resize(1);
  tx.vout.resize(1);
  CTransaction prev_tx(tx);

  CTransaction p2pkh = CreateP2PKHTx(prev_tx, k, 10000);
  uint160 validator_address = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validator_address) == false);
}

BOOST_AUTO_TEST_CASE(ExtractValidatorIndex_vote_fails) {

  Vote vote{};

  CKey k;
  InsecureNewKey(k, true);

  CTransaction p2pkh = CreateVoteTx(vote, k);
  uint160 validator_address = uint160();
  BOOST_CHECK(ExtractValidatorAddress(p2pkh, validator_address) == false);
}

BOOST_AUTO_TEST_SUITE_END()
