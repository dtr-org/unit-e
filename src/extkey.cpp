// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <extkey.h>
#include <key.h>

#include <cstdio>
#include <sstream>

const std::string DEFAULT_ACCOUNT_PATH("m/44'/2019'/0'");

constexpr char ERR_PATH_STR_EMPTY[] = "Path string empty";
constexpr char ERR_INT_INVALID_CHAR[] = "Integer conversion invalid character";
constexpr char ERR_MALFORMED_PATH[] = "Malformed path";
constexpr char ERR_OFFSET_HARDENED[] = "Offset is hardened already";


bool ParseExtKeyPath(const std::string &s, std::vector<uint32_t> &path, std::string &error) {
  path.clear();

  if (s.length() < 1) {
    error = ERR_PATH_STR_EMPTY;
    return false;
  }

  const auto npos = std::string::npos;
  for (size_t start = 0, end = 0; end != npos; start = end + 1) {
    end = s.find('/', start);
    size_t token_length = (end == npos) ? npos : (end - start);
    std::string token = s.substr(start, token_length);

    if (token.size() == 0) {
      error = ERR_MALFORMED_PATH;
      return false;
    }

    if (token == "m") {
      if (path.size() > 0 || start > 0) {
        error = ERR_MALFORMED_PATH;
        return false;
      }
      // Ignore initial 'm'
      continue;
    }

    std::istringstream is(std::move(token));

    uint32_t child;
    is >> child;
    if (is.fail()) {
      error = ERR_INT_INVALID_CHAR;
      return false;
    }

    char hardened = is.get();
    if (!is.eof()) {
      if (hardened != '\'' && hardened != 'h') {
        error = ERR_INT_INVALID_CHAR;
        return false;
      }
      if (child >= BIP32_HARDENED_KEY_LIMIT) {
        error = ERR_OFFSET_HARDENED;
        return false;
      }
      child |= BIP32_HARDENED_KEY_LIMIT;
    }

    // must consume the whole token
    if (is.peek() != EOF) {
      error = ERR_MALFORMED_PATH;
      return false;
    }

    path.emplace_back(child);
  }

  return true;
}

std::string FormatExtKeyPath(const std::vector<uint32_t> &path) {
  std::ostringstream s;
  s << "m";
  for (auto i : path) {
    s << '/' << (i & ~BIP32_HARDENED_KEY_LIMIT);
    if (i & BIP32_HARDENED_KEY_LIMIT) {
      s << '\'';
    }
  }
  return s.str();
}

std::string ExtKeyToString(const CExtPubKey &epk) {
  unsigned char code[BIP32_EXTKEY_SIZE];
  epk.Encode(code);
  return HexStr(code, code + sizeof(code));
}
