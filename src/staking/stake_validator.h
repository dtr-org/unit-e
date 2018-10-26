// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_STAKING_STAKE_VALIDATOR_H
#define UNIT_E_STAKING_STAKE_VALIDATOR_H

namespace staking {

class StakeValidator {
 public:
  virtual bool CheckBlock() = 0;

  static std::unique_ptr<StakeValidator> MakeStakeValidator();
};

}  // namespace staking

#endif  //UNIT_E_STAKING_STAKE_VALIDATOR_H
