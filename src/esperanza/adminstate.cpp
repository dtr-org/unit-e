// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/adminstate.h>

namespace esperanza {

AdminState::AdminState(const AdminParams &adminParams)
    : m_adminParams(adminParams),
      m_permissioningIsActive(!adminParams.m_blockToAdminKeys.empty()) {}

void AdminState::OnBlock(blockchain::Height blockHeight) {
  if (!m_permissioningIsActive) {
    return;
  }

  const auto adminIt = m_adminParams.m_blockToAdminKeys.find(blockHeight);
  if (adminIt != m_adminParams.m_blockToAdminKeys.end()) {
    ResetAdmin(adminIt->second);
  }

  const auto whiteListIt = m_adminParams.m_blockToWhiteList.find(blockHeight);
  if (whiteListIt != m_adminParams.m_blockToWhiteList.end()) {
    m_whiteList.clear();
    for (auto const &entry : whiteListIt->second) {
      m_whiteList.insert(entry);
    }
  }
}

bool AdminState::IsAdminAuthorized(const AdminKeySet &keys) const {
  if (!m_permissioningIsActive) {
    return false;
  }

  return keys == m_adminPubKeys;
}

bool AdminState::IsValidatorAuthorized(const uint160 &validatorAddress) const {
  if (!m_permissioningIsActive) {
    return true;
  }

  const auto it = m_whiteList.find(validatorAddress);
  return it != m_whiteList.end();
}

void AdminState::ResetAdmin(const AdminKeySet &newKeys) {
  m_adminPubKeys = newKeys;
}

void AdminState::AddValidator(const uint160 &validatorAddress) {
  m_whiteList.insert(validatorAddress);
}

void AdminState::RemoveValidator(const uint160 &validatorAddress) {
  m_whiteList.erase(validatorAddress);
}

void AdminState::EndPermissioning() { m_permissioningIsActive = false; }

bool AdminState::IsPermissioningActive() const {
  return m_permissioningIsActive;
}

const AdminParams &AdminState::GetParams() const {
  return m_adminParams;
}

bool AdminState::operator==(const AdminState &other) const {
  return m_adminPubKeys == other.m_adminPubKeys &&
         m_whiteList == other.m_whiteList &&
         m_permissioningIsActive == other.m_permissioningIsActive;
}

}  // namespace esperanza
