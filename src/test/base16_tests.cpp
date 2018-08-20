// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite.h>

#include <utilstrencodings.h>

#include <boost/test/unit_test.hpp>
#include <string>

#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(base16_tests, BasicTestingSetup)

template <std::size_t length>
std::vector<uint8_t> toUtf8Vector(const char (&text)[length]) {
  std::vector<uint8_t> vector;
  vector.reserve(length - 1);
  for (std::size_t i = 0; i < length - 1;
       ++i) {  // exclude null terminating byte
    vector.push_back(text[i]);
  }
  return vector;
}

BOOST_AUTO_TEST_CASE(encode_base16) {
  std::vector<uint8_t> input = toUtf8Vector(u8"안녕하세요, 당신은 어떠세요?");
  const std::string output = EncodeBase16(input);
  BOOST_CHECK_EQUAL(output,
                    "ec9588eb8595ed9598ec84b8ec9a942c20eb8bb9ec8ba0ec9d8020ec96"
                    "b4eb96a0ec84b8ec9a943f");
}

BOOST_AUTO_TEST_CASE(decode_base16) {
  const std::string input =
      "ec9588eb8595ed9598ec84b8ec9a942c20eb8bb9ec8ba0ec9d8020ec96b4eb96a0ec84b8"
      "ec9a943f";
  std::vector<uint8_t> output;
  const bool result = DecodeBase16(input, output);
  BOOST_CHECK_EQUAL(result, true);
  const std::vector<uint8_t> expectedOutput =
      toUtf8Vector(u8"안녕하세요, 당신은 어떠세요?");
  const std::vector<uint8_t> actualOutput = output;
  BOOST_CHECK(actualOutput == expectedOutput);
}

BOOST_AUTO_TEST_CASE(decode_base16_fail) {
  const std::string input = "this is not base16 encoded";
  std::vector<uint8_t> output;
  const bool result = DecodeBase16(input, output);
  BOOST_CHECK_EQUAL(result, false);
}

BOOST_AUTO_TEST_SUITE_END()
