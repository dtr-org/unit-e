// Copyright (c) 2014-2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KEY_MNEMONIC_H
#define KEY_MNEMONIC_H

#include <string>
#include <vector>

#include <base58.h>
#include <key.h>
#include <pubkey.h>

namespace key
{
namespace mnemonic
{

enum Language
{
    WLL_ENGLISH         = 1,
    WLL_FRENCH          = 2,
    WLL_JAPANESE        = 3,
    WLL_SPANISH         = 4,
    WLL_CHINESE_S       = 5,
    WLL_CHINESE_T       = 6,
    WLL_ITALIAN         = 7,
    WLL_KOREAN          = 8,

    WLL_MAX
};

int GetWord(int o, const char *pwl, int max, std::string &sWord);

int GetWordOffset(const char *p, const char *pwl, int max, int &o);

/*! Given a string of space separated words determines the language from the known wordlists.
 *
 * @return 0 if the language could not be detected, or a positive integer (can be cast to enum Language).
 */
int DetectLanguage(const std::string &sWordList);

int Encode(int nLanguage, const std::vector<uint8_t> &vEntropy, std::string &sWordList, std::string &sError);

int Decode(int nLanguage, const std::string &sWordListIn, std::vector<uint8_t> &vEntropy, std::string &sError,
           bool fIgnoreChecksum = false);

int ToSeed(const std::string &sMnemonic, const std::string &sPasswordIn, std::vector<uint8_t> &vSeed);

int AddChecksum(int nLanguageIn, const std::string &sWordListIn, std::string &sWordListOut, std::string &sError);

int GetWord(int nLanguage, int nWord, std::string &sWord, std::string &sError);

/*! \brief A Seed generated from a mnemonic of human-rememberable words.
 *
 * If the seed is not well formed
 *
 * TODO: Create a constructor that takes a language and an entropy source and generates
 * a seed from it.
 */
class Seed final {

private:
    int m_language;

    std::vector<uint8_t> m_seed;
    std::vector<uint8_t> m_entropy;
    std::string m_hexSeed;

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

    //! The master key for the hierarchical wallet (an extended key)
    const CExtKey& GetExtKey() const;

    //! A Base58 representation of the extended key (including checksum etc.)
    const CUnitEExtKey& GetExtKey58() const;
};

} // namespace mnemonic

} // namespace key

#endif // KEY_MNEMONIC_H
