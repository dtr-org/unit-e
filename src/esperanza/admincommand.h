// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_ESPERANZA_ADMINCOMMAND_H
#define UNITE_ESPERANZA_ADMINCOMMAND_H

#include <better-enums/enum.h>
#include <pubkey.h>
#include <script/script.h>
#include <serialize.h>
#include <stdint.h>
#include <vector>

namespace esperanza {

// clang-format off
BETTER_ENUM(
  AdminCommandType,
  uint8_t,
  ADD_TO_WHITELIST = 0,
  REMOVE_FROM_WHITELIST = 1,
  RESET_ADMINS = 2,
  END_PERMISSIONING = 3
)
// clang-format on

class AdminCommand {
 public:
  AdminCommand(const AdminCommandType &commandType,
               const std::vector<CPubKey> &payload);

  AdminCommand();

  AdminCommandType GetCommandType() const;

  //! Depending on AdminCommandType it has different meaning
  //! ADD_TO_WHITELIST - validator public keys to whitelist
  //! REMOVE_FROM_WHITELIST - validator public keys to remove from whitelist
  //! RESET_ADMINS - ADMIN_MULTISIG_KEYS new administrator public keys.
  //! END_PERMISSIONING - should be empty
  const std::vector<CPubKey> &GetPayload() const;

  ADD_SERIALIZE_METHODS;

  template <typename Stream, typename Operation>
  void SerializationOp(Stream &s, Operation ser_action) {
    READWRITE(FLATDATA(m_commandType));
    READWRITE(m_payload);
  }

  bool IsValid() const;

 private:
  AdminCommandType m_commandType;
  std::vector<CPubKey> m_payload;
};

bool MatchAdminCommand(const CScript &script);
bool DecodeAdminCommand(const CScript &script, AdminCommand &outAdminCommand);

CScript EncodeAdminCommand(const AdminCommand &command);

}  // namespace esperanza

#endif  // UNITE_ESPERANZA_ADMINCOMMAND_H
