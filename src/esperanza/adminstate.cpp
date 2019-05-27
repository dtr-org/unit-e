// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/adminstate.h>
#include <util/system.h>

namespace esperanza {

AdminState::AdminState(const AdminParams &params)
    : m_white_list(params.white_list.begin(), params.white_list.end()) {
  if (params.admin_keys) {
    m_permissioning_is_active = true;
    m_admin_pub_keys = params.admin_keys.get();
    for (const CPubKey &key : m_admin_pub_keys) {
      assert(key.IsFullyValid());
    }
  } else {
    m_permissioning_is_active = false;
    assert(m_white_list.empty());
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

bool AdminState::operator==(const AdminState &other) const {
  return m_admin_pub_keys == other.m_admin_pub_keys &&
         m_white_list == other.m_white_list &&
         m_permissioning_is_active == other.m_permissioning_is_active;
}

std::string AdminState::ToString() const {
  return strprintf(
      "AdminState{"
      "m_admin_pub_keys=%s "
      "m_white_list=%s "
      "m_permissioning_is_active=%d}",
      util::to_string(m_admin_pub_keys),
      util::to_string(m_white_list),
      m_permissioning_is_active);
}

}  // namespace esperanza
