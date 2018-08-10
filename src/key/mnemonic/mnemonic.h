// Copyright (c) 2014-2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MNEMONIC_H
#define MNEMONIC_H

#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace key
{
namespace mnemonic
{

enum class Language : uint8_t
{
    ENGLISH,
    FRENCH,
    JAPANESE,
    SPANISH,
    CHINESE_S,
    CHINESE_T,
    ITALIAN,
    KOREAN,

    COUNT // hack to automatically find the number of elements in the enumeration
};

int GetWord(int o, const char *pwl, int max, std::string &sWord);

int GetWordOffset(const char *p, const char *pwl, int max, int &o);

boost::optional<Language> DetectLanguage(const std::string &sWordList);

int Encode(Language language, const std::vector<uint8_t> &vEntropy, std::string &sWordList, std::string &sError);

int Decode(Language language, const std::string &sWordListIn, std::vector<uint8_t> &vEntropy, std::string &sError,
           bool fIgnoreChecksum = false);

int ToSeed(const std::string &sMnemonic, const std::string &sPasswordIn, std::vector<uint8_t> &vSeed);

int AddChecksum(Language language, const std::string &sWordListIn, std::string &sWordListOut, std::string &sError);

int GetWord(Language language, int nWord, std::string &sWord, std::string &sError);

} // namespace mnemonic

} // namespace key

#endif // MNEMONIC_H
