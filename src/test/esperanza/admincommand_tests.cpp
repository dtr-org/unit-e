// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addrman.h>
#include <esperanza/admincommand.h>
#include <test/test_unite.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(admincommand_tests, ReducedTestingSetup)

esperanza::AdminCommand CreateTestCommand() {
  CKey key1;
  key1.MakeNewKey(true);

  CKey key2;
  key2.MakeNewKey(true);

  std::vector<CPubKey> pubkeys = {key1.GetPubKey(), key2.GetPubKey()};

  return esperanza::AdminCommand(esperanza::AdminCommandType::ADD_TO_WHITELIST,
                                 pubkeys);
}

BOOST_AUTO_TEST_CASE(encode_decode_script_test) {
  auto srcCommand = CreateTestCommand();

  CScript script = EncodeAdminCommand(srcCommand);
  esperanza::AdminCommand dstCommand;
  BOOST_CHECK(TryDecodeAdminCommand(script, dstCommand));

  BOOST_CHECK_EQUAL(srcCommand.GetCommandType(), dstCommand.GetCommandType());
  BOOST_CHECK(srcCommand.GetPayload() == dstCommand.GetPayload());
}

BOOST_AUTO_TEST_CASE(decode_trimmed_script_test) {
  auto srcCommand = CreateTestCommand();

  CScript validScript = EncodeAdminCommand(srcCommand);

  for (size_t len = 0; len < validScript.size(); ++len) {
    CScript invalidScript(validScript.begin(), validScript.begin() + len);
    esperanza::AdminCommand command;
    BOOST_CHECK(!TryDecodeAdminCommand(invalidScript, command));
  }
}

BOOST_AUTO_TEST_CASE(decode_garbage_test) {
  std::initializer_list<uint8_t> bytes = {0x23, 0xFF};

  CScript script = CScript() << OP_RETURN << bytes;
  esperanza::AdminCommand command;
  BOOST_CHECK(!TryDecodeAdminCommand(script, command));
}

BOOST_AUTO_TEST_CASE(decode_garbage_test2) {
  std::initializer_list<uint8_t> bytes = {0x23, 0x00};

  CScript script = CScript() << OP_RETURN << bytes;
  esperanza::AdminCommand command;
  BOOST_CHECK(!TryDecodeAdminCommand(script, command));
}

BOOST_AUTO_TEST_SUITE_END()
