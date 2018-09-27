// Copyright (c) 2014-2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/hmac_sha512.h>
#include <crypto/sha256.h>
#include <util.h>

#include <unilib/uninorms.h>
#include <unilib/utf8.h>

#include <utilstrencodings.h>

#include <key/mnemonic/mnemonic.h>

#include <key/mnemonic/chinese_simplified.h>
#include <key/mnemonic/chinese_traditional.h>
#include <key/mnemonic/english.h>
#include <key/mnemonic/french.h>
#include <key/mnemonic/italian.h>
#include <key/mnemonic/japanese.h>
#include <key/mnemonic/korean.h>
#include <key/mnemonic/spanish.h>

namespace key {
namespace mnemonic {

static const unsigned char* mnLanguages[] = {
    english_txt,
    french_txt,
    japanese_txt,
    spanish_txt,
    chinese_simplified_txt,
    chinese_traditional_txt,
    italian_txt,
    korean_txt,
};

static const uint32_t mnLanguageLens[] = {
    english_txt_len,
    french_txt_len,
    japanese_txt_len,
    spanish_txt_len,
    chinese_simplified_txt_len,
    chinese_traditional_txt_len,
    italian_txt_len,
    korean_txt_len,
};

static const std::string
    languagesDesc[static_cast<uint16_t>(Language::_size_constant)] = {
        "English",
        "French",
        "Japanese",
        "Spanish",
        "Chinese Simplified",
        "Chinese Traditional",
        "Italian",
        "Korean",
};

static const std::string
    languagesTags[static_cast<uint16_t>(Language::_size_constant)] = {
        "english",   "french",    "japanese", "spanish",
        "chinese_s", "chinese_t", "italian",  "korean",
};

static void ReplaceStrInPlace(std::string& subject, const std::string search,
                              const std::string replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

static std::string& LTrimWhitespace(std::string& s) {
  std::string::iterator i;
  for (i = s.begin(); i != s.end(); ++i) {
    if (!std::isspace(*i)) {
      break;
    }
  }
  if (i != s.begin()) {
    s.erase(s.begin(), i);
  }
  return s;
}

static std::string& RTrimWhitespace(std::string& s) {
  std::string::reverse_iterator i;
  for (i = s.rbegin(); i != s.rend(); ++i) {
    if (!std::isspace(*i)) {
      break;
    }
  }
  if (i != s.rbegin()) {
    s.erase(i.base(), s.end());
  }
  return s;
}

static std::string& TrimWhitespace(std::string& s) {
  LTrimWhitespace(s);
  RTrimWhitespace(s);
  return s;
}

static void NormaliseUnicode(std::string& str) {
  std::u32string u32;
  ufal::unilib::utf8::decode(str, u32);
  ufal::unilib::uninorms::nfkd(u32);
  ufal::unilib::utf8::encode(u32, str);
}

static void NormaliseInput(std::string& str) {
  TrimWhitespace(str);
  NormaliseUnicode(str);
}

std::string GetLanguageDesc(const Language language) {
  return languagesDesc[static_cast<int>(language)];
}

std::string GetLanguageTag(const Language language) {
  return languagesTags[static_cast<int>(language)];
}

int GetWord(int o, const char* pwl, int max, std::string& sWord) {
  sWord = "";
  char* pt = (char*)pwl;
  while (o > 0) {
    if (*pt == '\n') {
      o--;
    }
    pt++;

    if (pt >= pwl + max) {
      return 1;
    }
  }
  while (pt < (pwl + max)) {
    if (*pt == '\n') {
      return 0;
    }
    sWord += *pt;
    pt++;
  }

  return 1;
}

int GetWordOffset(const char* p, const char* pwl, int max, int& o) {
  // List must end with \n
  char* pt = (char*)pwl;
  int l = strlen(p);
  int i = 0;
  int c = 0;
  int f = 1;
  while (pt < (pwl + max)) {
    if (*pt == '\n') {
      if (f && c == l) {  // found
        o = i;
        return 0;
      }
      i++;
      c = 0;
      f = 1;
    } else {
      if (c >= l) {
        f = 0;
      } else if (f && *(p + c) != *pt) {
        f = 0;
      }
      c++;
    }
    pt++;
  }
  return 1;
}

boost::optional<Language> DetectLanguage(const std::string& sWordList) {
  char tmp[2048];
  if (sWordList.size() >= 2048) {
    return boost::none;
  }

  // try to detect the language
  // try max n words
  // allow errors, tolerate spelling mistakes, mistakes will be reported in
  // other functions
  for (size_t l = 0; l < Language::_size_constant; ++l) {
    strcpy(tmp, sWordList.c_str());

    char* pwl = (char*)mnLanguages[l];
    int m = mnLanguageLens[l];

    // The chinese dialects have many words in common, match full phrase
    int maxTries = (l == static_cast<int>(Language::CHINESE_S) ||
                    l == static_cast<int>(Language::CHINESE_T))
                       ? 24
                       : 8;

    int nHit = 0;
    int nMiss = 0;
    char* p;
    p = strtok(tmp, " ");
    while (p != nullptr) {
      int ofs;
      if (0 == GetWordOffset(p, pwl, m, ofs)) {
        nHit++;
      } else {
        nMiss++;
      }
      if (!maxTries--) {
        break;
      }
      p = strtok(nullptr, " ");
    }
    // Chinese dialects overlap too much to tolerate failures
    if ((l == static_cast<int>(Language::CHINESE_S) ||
         l == static_cast<int>(Language::CHINESE_T)) &&
        nMiss > 0) {
      continue;
    }
    if (nHit > nMiss && nMiss < 2) {  // tolerate max 2 failures
      return Language::_from_index(l);
    }
  }

  return boost::none;
}

int Encode(Language language, const std::vector<uint8_t>& vEntropy,
           std::string& sWordList, std::string& sError) {
  sWordList = "";

  const int nLanguage = language._to_index();

  // Checksum is 1st n bytes of the sha256 hash
  uint8_t hash[32];
  CSHA256().Write(&vEntropy[0], vEntropy.size()).Finalize((uint8_t*)hash);

  int nCsSize = vEntropy.size() / 4;  // 32 / 8
  if (nCsSize < 1 || nCsSize > 256) {
    sError = "Entropy bytes out of range.";
    return error<2>("%s: %s", __func__, sError.c_str());
  }

  std::vector<uint8_t> vIn = vEntropy;

  int ncb = nCsSize / 8;
  int r = nCsSize % 8;
  if (r != 0) {
    ncb++;
  }
  std::vector<uint8_t> vTmp(32);
  memcpy(&vTmp[0], &hash, ncb);
  memset(&vTmp[ncb], 0, 32 - ncb);

  vIn.insert(vIn.end(), vTmp.begin(), vTmp.end());

  std::vector<int> vWord;

  int nBits = vEntropy.size() * 8 + nCsSize;

  int i = 0;
  while (i < nBits) {
    int o = 0;
    int s = i / 8;
    int r = i % 8;

    uint8_t b1 = vIn[s];
    uint8_t b2 = vIn[s + 1];

    o = (b1 << r) & 0xFF;
    o = o << (11 - 8);

    if (r > 5) {
      uint8_t b3 = vIn[s + 2];
      o |= (b2 << (r - 5));
      o |= (b3 >> (8 - (r - 5)));
    } else {
      o |= ((int)b2) >> ((8 - (11 - 8)) - r);
    }

    o = o & 0x7FF;

    vWord.push_back(o);
    i += 11;
  }

  char* pwl = (char*)mnLanguages[nLanguage];
  int m = mnLanguageLens[nLanguage];

  for (size_t k = 0; k < vWord.size(); ++k) {
    int o = vWord[k];

    std::string sWord;
    if (0 != GetWord(o, pwl, m, sWord)) {
      sError = strprintf("Word extract failed %d, language %d.", o, nLanguage);
      return error<3>("%s: %s", __func__, sError.c_str());
    }

    if (sWordList != "") {
      sWordList += " ";
    }
    sWordList += sWord;
  }

  if (nLanguage == static_cast<int>(Language::JAPANESE)) {
    ReplaceStrInPlace(sWordList, " ", "\u3000");
  }

  return 0;
}

int Decode(Language language, const std::string& sWordListIn,
           std::vector<uint8_t>& vEntropy, std::string& sError,
           bool fIgnoreChecksum) {
  const int nLanguage = static_cast<int>(language);

  std::string sWordList = sWordListIn;
  NormaliseInput(sWordList);

  char tmp[2048];
  if (sWordList.size() >= 2048) {
    sError = "Word List too long.";
    return error<2>("%s: %s", __func__, sError.c_str());
  }

  strcpy(tmp, sWordList.c_str());

  char* pwl = (char*)mnLanguages[nLanguage];
  int m = mnLanguageLens[nLanguage];

  std::vector<int> vWordInts;

  char* p;
  p = strtok(tmp, " ");
  while (p != nullptr) {
    int ofs;
    if (0 != GetWordOffset(p, pwl, m, ofs)) {
      sError = strprintf("Unknown word: %s", p);
      return error<3>("%s: %s", __func__, sError.c_str());
    }

    vWordInts.push_back(ofs);

    p = strtok(nullptr, " ");
  }

  if (!fIgnoreChecksum && vWordInts.size() % 3 != 0) {
    sError = "No. of words must be divisible by 3.";
    return error<4>("%s: %s", __func__, sError.c_str());
  }

  int nBits = vWordInts.size() * 11;
  int nBytes = nBits / 8 + (nBits % 8 == 0 ? 0 : 1);
  vEntropy.resize(nBytes);

  memset(&vEntropy[0], 0, nBytes);

  int i = 0;
  size_t wl = vWordInts.size();
  size_t el = vEntropy.size();
  for (size_t k = 0; k < wl; ++k) {
    int o = vWordInts[k];

    int s = i / 8;
    int r = i % 8;

    vEntropy[s] |= (o >> (r + 3)) & 0x7FF;

    if (s < (int)el - 1) {
      if (r > 5) {
        vEntropy[s + 1] |= ((o >> (r - 5))) & 0x7FF;
        if (s < (int)el - 2) {
          vEntropy[s + 2] |= (o << (8 - (r - 5))) & 0x7FF;
        }
      } else {
        vEntropy[s + 1] |= (o << (5 - r)) & 0x7FF;
      }
    }
    i += 11;
  }

  if (fIgnoreChecksum) {
    return 0;
  }

  // Checksum
  int nLenChecksum = nBits / 32;
  int nLenEntropy = nBits - nLenChecksum;

  int nBytesEntropy = nLenEntropy / 8;
  int nBytesChecksum = nLenChecksum / 8 + (nLenChecksum % 8 == 0 ? 0 : 1);

  std::vector<uint8_t> vCS;

  vCS.resize(nBytesChecksum);
  memcpy(&vCS[0], &vEntropy[nBytesEntropy], nBytesChecksum);

  vEntropy.resize(nBytesEntropy);

  uint8_t hash[32];
  CSHA256().Write(&vEntropy[0], vEntropy.size()).Finalize((uint8_t*)hash);

  std::vector<uint8_t> vCSTest;

  vCSTest.resize(nBytesChecksum);
  memcpy(&vCSTest[0], &hash, nBytesChecksum);

  int r = nLenChecksum % 8;

  if (r > 0) {
    vCSTest[nBytesChecksum - 1] &= (((1 << r) - 1) << (8 - r));
  }
  if (vCSTest != vCS) {
    sError = "Checksum mismatch.";
    return error<5>("%s: %s", __func__, sError.c_str());
  }

  return 0;
}

static int mnemonicKdf(const uint8_t* password, size_t lenPassword,
                       const uint8_t* salt, size_t lenSalt, size_t nIterations,
                       uint8_t* out) {
  /*
  https://tools.ietf.org/html/rfc2898
  5.2 PBKDF2

  F (P, S, c, i) = U_1 \xor U_2 \xor ... \xor U_c
  where
      U_1 = PRF (P, S || INT (i)) ,
      U_2 = PRF (P, U_1) ,
      ...
      U_c = PRF (P, U_{c-1}) .
  */

  // output length is always 64bytes, only 1 block

  if (nIterations < 1) {
    return 1;
  }

  uint8_t r[64];

  int one = 0x01000000;
  CHMAC_SHA512 ctx(password, lenPassword);
  CHMAC_SHA512 ctx_state = ctx;
  ctx.Write(salt, lenSalt);
  ctx.Write((uint8_t*)&one, 4);
  ctx.Finalize(r);
  memcpy(out, r, 64);

  for (size_t k = 1; k < nIterations; ++k) {
    ctx = ctx_state;
    ctx.Write(r, 64);
    ctx.Finalize(r);

    for (size_t i = 0; i < 64; ++i) {
      out[i] ^= r[i];
    }
  }

  return 0;
}

int ToSeed(const std::string& sMnemonic, const std::string& sPasswordIn,
           std::vector<uint8_t>& vSeed) {
  vSeed.resize(64);

  std::string sWordList = sMnemonic;
  NormaliseInput(sWordList);

  std::string sPassword = sPasswordIn;
  NormaliseInput(sPassword);

  int nIterations = 2048;

  std::string sSalt = std::string("mnemonic") + sPassword;

  if (0 != mnemonicKdf((uint8_t*)sWordList.data(), sWordList.size(),
                       (uint8_t*)sSalt.data(), sSalt.size(), nIterations,
                       &vSeed[0])) {
    return error<1>("%s: mnemonicKdf failed.", __func__);
  }
  return 0;
}

int AddChecksum(Language language, const std::string& sWordListIn,
                std::string& sWordListOut, std::string& sError) {
  sWordListOut = "";
  int rv;
  std::vector<uint8_t> vEntropy;
  if (0 != (rv = Decode(language, sWordListIn, vEntropy, sError, true))) {
    return rv;
  }
  if (0 != (rv = Encode(language, vEntropy, sWordListOut, sError))) {
    return rv;
  }
  if (0 != (rv = Decode(language, sWordListOut, vEntropy, sError))) {
    return rv;
  }
  return 0;
}

int GetWord(Language language, int nWord, std::string& sWord,
            std::string& sError) {
  const int nLanguage = static_cast<int>(language);
  char* pwl = (char*)mnLanguages[nLanguage];
  int m = mnLanguageLens[nLanguage];

  if (0 != GetWord(nWord, pwl, m, sWord)) {
    sError =
        strprintf("Word extract failed %d, language %d.", nWord, nLanguage);
    return error<3>("%s: %s", __func__, sError.c_str());
  }

  return 0;
}

Seed::Seed(const std::string& mnemonic, const std::string& passphrase) {
  boost::optional<Language> maybeLanguage = DetectLanguage(mnemonic);
  if (boost::none == maybeLanguage) {
    throw std::runtime_error(
        "invalid mnemonic: did not detect a known language");
  }
  m_language = maybeLanguage.get();
  std::string error;
  if (0 != Decode(m_language, mnemonic, m_entropy, error)) {
    throw std::runtime_error(strprintf("invalid mnemonic: %s", error.c_str()));
  }
  if (0 != ToSeed(mnemonic, passphrase, m_seed)) {
    // this should never happen as the previous if statement already checks
    // whether the mnemonic can be decoded.
    throw std::runtime_error(
        strprintf("invalid mnemonic: %s", mnemonic.c_str()));
  }
  m_hexSeed = EncodeBase16(m_seed);
  m_hexEntropy = EncodeBase16(m_entropy);
  m_extKey.SetMaster(m_seed.data(), static_cast<unsigned int>(m_seed.size()));
  m_extKey58.SetKey(m_extKey);
}

const std::string& Seed::GetHumandReadableLanguage() const {
  return languagesDesc[static_cast<int>(m_language)];
}

const std::string& Seed::GetLanguageTag() const {
  return languagesTags[static_cast<int>(m_language)];
}

const std::string& Seed::GetHexSeed() const { return m_hexSeed; }

const std::string& Seed::GetHexEntropy() const { return m_hexEntropy; }

const CExtKey& Seed::GetExtKey() const { return m_extKey; }

const CUnitEExtKey& Seed::GetExtKey58() const { return m_extKey58; }

}  // namespace mnemonic

}  // namespace key
