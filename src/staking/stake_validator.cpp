// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <staking/stake_validator.h>

namespace staking {

class StakeValidatorImpl : public StakeValidator {

 private:
  uint256 ComputeStakeModifier() {
  }

 public:
  bool CheckBlock() {

  }
};

std::unique_ptr<StakeValidator> StakeValidator::MakeStakeValidator() {

}

}  // namespace staking
