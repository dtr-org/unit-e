// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_SCRIPT_UTIL_H
#define UNIT_E_SCRIPT_UTIL_H

#include <script/script.h>

#include <array>
#include <cstdint>

namespace script {

//! \brief Format an opcode, i.e. return OP_EQUALVERIFY for 0x88
std::string Prettify(opcodetype opcode);

//! \brief Pretty-print a script
std::string Prettify(const CScript &script);

}  // namespace script

#endif  //UNIT_E_SCRIPT_UTIL_H
