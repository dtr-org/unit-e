#include <base58.h>
#include <key.h>
#include <key/extkey.h>

#include <cstdio>
#include <sstream>

namespace extkey {

std::string GetDefaultAccountPathString() {
  char buffer[32];
  int size = sprintf(buffer, "m/44'/%d'/0'", BIP44_COIN_TYPE);

  return std::string(buffer, buffer + size);
}

static const char *ERR_PATH_STR_EMPTY = "Path string empty";
static const char *ERR_INT_INVALID_CHAR = "Integer conversion invalid character";
static const char *ERR_MALFORMED_PATH = "Malformed path";
static const char *ERR_OFFSET_HARDENED = "Offset is hardened already";

bool ParsePath(const std::string &s, std::vector<uint32_t> &path, std::string &error) {
  path.clear();

  if (s.length() < 1) {
    error = ERR_PATH_STR_EMPTY;
    return false;
  }

  const auto npos = std::string::npos;
  for (size_t start = 0, end = 0; end != npos; start = end + 1) {
    end = s.find('/', start);
    int token_length = (end == npos) ? npos : (end - start);
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

std::string FormatPath(const std::vector<uint32_t> &path) {
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

std::string ToString(const CExtPubKey &epk) {
  unsigned char code[BIP32_EXTKEY_SIZE];
  epk.Encode(code);
  return HexStr(code, code + sizeof(code));
}

}  // namespace extkey
