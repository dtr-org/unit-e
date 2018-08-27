// Copyright (c) 2014-2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_KEY_MNEMONIC_MNEMONIC_H
#define UNITE_KEY_MNEMONIC_MNEMONIC_H

#include <string>
#include <vector>

#include <base58.h>
#include <key.h>

#include <boost/optional.hpp>

namespace key {
namespace mnemonic {

enum class Language : uint8_t {
  ENGLISH,
  FRENCH,
  JAPANESE,
  SPANISH,
  CHINESE_S,
  CHINESE_T,
  ITALIAN,
  KOREAN,

  COUNT  // hack to automatically find the number of elements in the enumeration
};

std::string GetLanguageDesc(const Language language);

std::string GetLanguageTag(const Language language);

int GetWord(int o, const char* pwl, int max, std::string& sWord);

int GetWordOffset(const char* p, const char* pwl, int max, int& o);

boost::optional<Language> DetectLanguage(const std::string& sWordList);

int Encode(Language language, const std::vector<uint8_t>& vEntropy,
           std::string& sWordList, std::string& sError);

int Decode(Language language, const std::string& sWordListIn,
           std::vector<uint8_t>& vEntropy, std::string& sError,
           bool fIgnoreChecksum = false);

int ToSeed(const std::string& sMnemonic, const std::string& sPasswordIn,
           std::vector<uint8_t>& vSeed);

int AddChecksum(Language language, const std::string& sWordListIn,
                std::string& sWordListOut, std::string& sError);

int GetWord(Language language, int nWord, std::string& sWord,
            std::string& sError);

/*! \brief A Seed generated from a mnemonic of human-rememberable words.
 *
 * If the seed is not well formed
 *
 * TODO: Create a constructor that takes a language and an entropy source and
 * generates a seed from it.
 */
class Seed final {
 private:
  Language m_language;
  std::vector<uint8_t> m_seed;
  std::vector<uint8_t> m_entropy;
  std::string m_hexSeed;
  std::string m_hexEntropy;
  CExtKey m_extKey;
  CUnitEExtKey m_extKey58;

 public:
  Seed(const std::string& mnemonic, const std::string& passphrase = "");

  //! The name of this language, human readable and nicely formatted
  const std::string& GetHumandReadableLanguage() const;

  //! A machine readable identifier for this language (all lowercase, no spaces)
  const std::string& GetLanguageTag() const;

  //! The seed in hexadecimal
  const std::string& GetHexSeed() const;

  //! The entropy in hexadecimal
  const std::string& GetHexEntropy() const;

  //! The master key for the hierarchical wallet (an extended key)
  const CExtKey& GetExtKey() const;

  //! A Base58 representation of the extended key (including checksum etc.)
  const CUnitEExtKey& GetExtKey58() const;
};

}  // namespace mnemonic

}  // namespace key

#endif  // UNITE_KEY_MNEMONIC_MNEMONIC_H
