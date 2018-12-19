// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_ADMINPARAMS_H
#define UNITE_ESPERANZA_ADMINPARAMS_H

#include <blockchain/blockchain_types.h>
#include <pubkey.h>
#include <uint256.h>
#include <array>
#include <map>

namespace esperanza {

constexpr size_t ADMIN_MULTISIG_SIGNATURES = 2;
constexpr size_t ADMIN_MULTISIG_KEYS = 3;

using AdminKeySet = std::array<CPubKey, ADMIN_MULTISIG_KEYS>;

//! \brief Esperanza Permissioning-specific blockchain parameters
struct AdminParams {
  std::map<blockchain::Height, AdminKeySet> m_blockToAdminKeys;
  std::map<blockchain::Height, std::vector<uint160>> m_blockToWhiteList;
};

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_ADMINPARAMS_H
