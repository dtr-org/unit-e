// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/adminstate.h>

namespace esperanza {

AdminState::AdminState(const AdminParams &adminParams)
    : m_admin_params(adminParams),
      m_permissioning_is_active(!adminParams.m_block_to_admin_keys.empty()) {}

void AdminState::OnBlock(blockchain::Height blockHeight) {
  if (!m_permissioning_is_active) {
    return;
  }

  const auto adminIt = m_admin_params.m_block_to_admin_keys.find(blockHeight);
  if (adminIt != m_admin_params.m_block_to_admin_keys.end()) {
    ResetAdmin(adminIt->second);
  }

  const auto whiteListIt = m_admin_params.m_block_to_white_list.find(blockHeight);
  if (whiteListIt != m_admin_params.m_block_to_white_list.end()) {
    m_white_list.clear();
    for (auto const &entry : whiteListIt->second) {
      m_white_list.insert(entry);
    }
  }
}

bool AdminState::IsAdminAuthorized(const AdminKeySet &keys) const {
  if (!m_permissioning_is_active) {
    return false;
  }

  return keys == m_admin_pub_keys;
}

bool AdminState::IsValidatorAuthorized(const uint160 &validatorAddress) const {
  if (!m_permissioning_is_active) {
    return true;
  }

  const auto it = m_white_list.find(validatorAddress);
  return it != m_white_list.end();
}

void AdminState::ResetAdmin(const AdminKeySet &newKeys) {
  m_admin_pub_keys = newKeys;
}

void AdminState::AddValidator(const uint160 &validatorAddress) {
  m_white_list.insert(validatorAddress);
}

void AdminState::RemoveValidator(const uint160 &validatorAddress) {
  m_white_list.erase(validatorAddress);
}

void AdminState::EndPermissioning() { m_permissioning_is_active = false; }

bool AdminState::IsPermissioningActive() const {
  return m_permissioning_is_active;
}

const AdminParams &AdminState::GetParams() const {
  return m_admin_params;
}

bool AdminState::operator==(const AdminState &other) const {
  return m_admin_pub_keys == other.m_admin_pub_keys &&
         m_white_list == other.m_white_list &&
         m_permissioning_is_active == other.m_permissioning_is_active;
}

}  // namespace esperanza
