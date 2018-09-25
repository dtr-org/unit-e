// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/admincommand.h>
#include <esperanza/adminparams.h>
#include <streams.h>

namespace esperanza {

AdminCommand::AdminCommand(const AdminCommandType &command_type,
                           const std::vector<CPubKey> &pubkeys)
    : m_command_type(command_type), m_pubkeys(pubkeys) {}

AdminCommand::AdminCommand()
    : m_command_type(+AdminCommandType::REMOVE_FROM_WHITELIST) {}

bool AdminCommand::IsValid() const {
  switch (m_command_type) {
    case AdminCommandType::END_PERMISSIONING:
      return m_pubkeys.empty();
    case AdminCommandType::ADD_TO_WHITELIST:
    case AdminCommandType::REMOVE_FROM_WHITELIST:
      if (m_pubkeys.empty()) {
        return false;
      }
      break;
    case AdminCommandType::RESET_ADMINS:
      if (m_pubkeys.size() != ADMIN_MULTISIG_KEYS) {
        return false;
      }
      break;
    default:
      return false;
  }

  for (const auto &key : m_pubkeys) {
    if (!key.IsValid() || !key.IsCompressed()) {
      return false;
    }
  }
  return true;
}

AdminCommandType AdminCommand::GetCommandType() const { return m_command_type; }

const std::vector<CPubKey> &AdminCommand::GetPubkeys() const {
  return m_pubkeys;
}

CScript EncodeAdminCommand(const AdminCommand &command) {
  CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);

  stream << command;

  std::vector<uint8_t> data(stream.begin(), stream.end());

  CScript script = CScript() << OP_RETURN << data;

  return script;
}

bool TryDecodeAdminCommand(const CScript &script,
                           AdminCommand &outAdminCommand) {
  opcodetype opcode;
  CScript::const_iterator it = script.begin();

  if (!script.GetOp(it, opcode) || opcode != OP_RETURN) {
    return false;
  }

  std::vector<uint8_t> data;
  if (!script.GetOp(it, opcode, data)) {
    return false;
  }

  try {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    stream >> outAdminCommand;

    return outAdminCommand.IsValid();
  } catch (const std::ios_base::failure &) {
    return false;
  }
}

}  // namespace esperanza
