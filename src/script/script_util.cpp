// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script_util.h>

#include <tinyformat.h>

#include <array>
#include <climits>
#include <cstdint>
#include <regex>
#include <string>
#include <sstream>

namespace script {

namespace {

struct OpCodeInfo {
  std::uint8_t code;
  std::string label;
};

constexpr unsigned int NUM_OPCODES = 256;

const std::array<OpCodeInfo, NUM_OPCODES> OP_CODE_INFO_ARR = [] {
  std::array<OpCodeInfo, NUM_OPCODES> arr{};
  std::string info(OP_CODE_INFO);
  std::regex opcode_definition("(OP_[A-Z0-9_]+) *= *(0x[0-9a-fA-F]+)");
  std::sregex_iterator begin(info.begin(), info.end(), opcode_definition);
  std::sregex_iterator end;
  std::array<bool, NUM_OPCODES> found{};
  found.fill(false);
  for (auto it = begin; it != end; ++it) {
    std::smatch match = *it;
    const auto opcode_label = std::string(match[1]);
    const auto opcode_value = static_cast<std::uint8_t>(std::stoul(match[2], nullptr, 0));
    arr[opcode_value].code = opcode_value;
    arr[opcode_value].label = opcode_label;
    found[opcode_value] = true;
  }
  for (unsigned int i = 0; i < NUM_OPCODES; ++i) {
    if (!found[i]) {
      arr[i].code = static_cast<uint8_t>(i);
      arr[i].label = tfm::format("<%d>", i);
    }
  }
  return arr;
}();

}  // namespace

std::string Prettify(opcodetype opcode) {
  return OP_CODE_INFO_ARR[opcode].label;
}

std::string Prettify(const CScript &script) {
  std::stringstream s;

  opcodetype op;
  std::vector<uint8_t> data;
  CScript::const_iterator it = script.begin();

  s << "Script{ ";
  while (script.GetOp(it, op, data)) {
    s << Prettify(op);
    if (!data.empty()) {
      s << " [";
      for (const auto b : data) {
        tfm::format(s, "%02x", b);
      }
      s << "]";
    }
    s << " ";
  }
  s << "}";

  return s.str();
}

}  // namespace script
