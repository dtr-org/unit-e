// Copyright (c) 2014-2015 The ShadowCoin developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef KEY_MNEMONIC_H
#define KEY_MNEMONIC_H

#include <string>
#include <vector>

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

extern const char *mnLanguagesDesc[WLL_MAX];
extern const char *mnLanguagesTag[WLL_MAX];

/*!
 *
 * @param o
 * @param pwl
 * @param max
 * @param sWord
 * @return
 */
int GetWord(int o, const char *pwl, int max, std::string &sWord);

/*!
 *
 * @param p
 * @param pwl
 * @param max
 * @param o
 * @return
 */
int GetWordOffset(const char *p, const char *pwl, int max, int &o);

/*!
 *
 * @param sWordList
 * @return
 */
int MnemonicDetectLanguage(const std::string &sWordList);

/*!
 *
 * @param nLanguage
 * @param vEntropy
 * @param sWordList
 * @param sError
 * @return
 */
int MnemonicEncode(int nLanguage, const std::vector<uint8_t> &vEntropy, std::string &sWordList, std::string &sError);

/*!
 *
 * @param nLanguage
 * @param sWordListIn
 * @param vEntropy
 * @param sError
 * @param fIgnoreChecksum
 * @return
 */
int MnemonicDecode(int nLanguage, const std::string &sWordListIn, std::vector<uint8_t> &vEntropy, std::string &sError, bool fIgnoreChecksum=false);

/*!
 *
 * @param sMnemonic
 * @param sPasswordIn
 * @param vSeed
 * @return
 */
int MnemonicToSeed(const std::string &sMnemonic, const std::string &sPasswordIn, std::vector<uint8_t> &vSeed);

/*!
 *
 * @param nLanguageIn
 * @param sWordListIn
 * @param sWordListOut
 * @param sError
 * @return
 */
int MnemonicAddChecksum(int nLanguageIn, const std::string &sWordListIn, std::string &sWordListOut, std::string &sError);

/*!
 *
 * @param nLanguage
 * @param nWord
 * @param sWord
 * @param sError
 * @return
 */
int MnemonicGetWord(int nLanguage, int nWord, std::string &sWord, std::string &sError);

} // namespace mnemonic

} // namespace key

#endif // KEY_MNEMONIC_H
