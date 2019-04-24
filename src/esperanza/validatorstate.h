// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_VALIDATORSTATE_H
#define UNITE_ESPERANZA_VALIDATORSTATE_H

#include <better-enums/enum.h>
#include <esperanza/vote.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <stdint.h>
#include <sync.h>
#include <uint256.h>
#include <map>

namespace esperanza {

// clang-format off
//! All phases are ordered in a way how they can progress.
//! Every phase has sparse index because if we want to introduce
//! a new one, we can include it in between without breaking
//! a layout on disk
BETTER_ENUM(
    _Phase,
    uint8_t,

    //! finalizer didn't send deposit
    NOT_VALIDATING = 10,

    //! deposit is in the mempool/wallet but is not included in a block
    WAITING_DEPOSIT_CONFIRMATION = 20,

    //! deposit is included in a block but start_dynasty hasn't begun
    WAITING_DEPOSIT_FINALIZATION = 30,

    //! finalizer is able to vote. Starts from Validator.m_start_dynasty
    //! and until logout delay passes
    IS_VALIDATING = 40,

    //! logout delay passed and we are in withdraw delay
    WAITING_FOR_WITHDRAW_DELAY = 50,

    //! withdraw delay passed but finalizer hasn't withdrawn yet
    WAITING_TO_WITHDRAW = 60
)
// clang-format on

class ValidatorState {
 public:
  using Phase = _Phase;

  uint160 m_validator_address = uint160S("0");

  //! store votes by target_epoch
  std::map<uint32_t, Vote> m_vote_map;

  //! is used to prevent creating double-deposits for the same wallet.
  //! once deposit is included in the block, current FinalizationState
  //! knows about this finalizer and we don't need this field anymore.
  uint256 m_last_deposit_tx;

  //! m_last_source_epoch and m_last_target_epoch are used to detect
  //! double or surrounded votes and skip voting for that epoch.
  uint32_t m_last_source_epoch = 0;
  uint32_t m_last_target_epoch = 0;

  ADD_SERIALIZE_METHODS

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(m_validator_address);
    READWRITE(m_vote_map);
    READWRITE(m_last_deposit_tx);
    READWRITE(m_last_source_epoch);
    READWRITE(m_last_target_epoch);
  }

  bool operator==(const ValidatorState &v) const {
    return m_validator_address == v.m_validator_address &&
           m_vote_map == v.m_vote_map &&
           m_last_deposit_tx == v.m_last_deposit_tx &&
           m_last_source_epoch == v.m_last_source_epoch &&
           m_last_target_epoch == v.m_last_target_epoch;
  }
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_VALIDATORSTATE_H
