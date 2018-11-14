// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_ADMINSTATE_H
#define UNITE_ESPERANZA_ADMINSTATE_H

#include <base58.h>
#include <esperanza/adminparams.h>
#include <key.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <sync.h>
#include <uint256.h>
#include <array>

namespace esperanza {

//! \brief Represents current administration state
//!
//! State includes:
//! - Validator whitelist
//! - Current administrator keys
//! - Whether permissioing era is over
class AdminState {
  AdminKeySet m_adminPubKeys;
  std::set<uint256> m_whiteList;
  const AdminParams &m_adminParams;
  bool m_permissioningIsActive;

 public:
  explicit AdminState(const AdminParams &adminParams);

  void OnBlock(int blockHeight);
  bool IsAdminAuthorized(const AdminKeySet &keys) const;
  bool IsValidatorAuthorized(const uint256 &validatorAddress) const;
  void ResetAdmin(const AdminKeySet &newKeys);
  void AddValidator(const uint256 &validatorAddress);
  void RemoveValidator(const uint256 &validatorAddress);
  void EndPermissioning();
  bool IsPermissioningActive() const;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_ADMINSTATE_H
