// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchain/blockchain_genesis.h>
#include <blockchain/blockchain_parameters.h>
#include <hash.h>
#include <staking/stake_validator.h>
#include <test/test_unite.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

namespace {

blockchain::Parameters p = blockchain::Parameters::MainNet();
std::unique_ptr<blockchain::Behavior> b = blockchain::Behavior::FromParameters(p);

}  // namespace

BOOST_AUTO_TEST_SUITE(stake_validator_tests)

BOOST_AUTO_TEST_CASE(check_kernel) {
  const auto stake_validator = staking::StakeValidator::New(b.get());
  const uint256 kernel;
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(p).nBits;
  BOOST_CHECK(stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(check_kernel_fail) {
  const auto stake_validator = staking::StakeValidator::New(b.get());
  const uint256 kernel = uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  const auto difficulty = blockchain::GenesisBlockBuilder().Build(p).nBits;
  BOOST_CHECK(!stake_validator->CheckKernel(1, kernel, difficulty));
}

BOOST_AUTO_TEST_CASE(remember_and_forget) {
  const auto stake_validator = staking::StakeValidator::New(b.get());
  const uint256 kernel = uint256S("000000000000000000000000e6b8347d447e02ed383a3e96986815d576fb2a5a");
  LOCK(stake_validator->GetLock());
  BOOST_CHECK(!stake_validator->IsKernelKnown(kernel));
  stake_validator->RememberKernel(kernel);
  BOOST_CHECK(stake_validator->IsKernelKnown(kernel));
  stake_validator->ForgetKernel(kernel);
  BOOST_CHECK(!stake_validator->IsKernelKnown(kernel));
}

BOOST_AUTO_TEST_SUITE_END()
