// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_UTILSTR_H
#define UNIT_E_UTILSTR_H

#include <util.h>
#include <amount.h>

namespace util
{
namespace str
{

void ReplaceStrInPlace(std::string &subject, const std::string search, const std::string replace);

bool IsStringBoolPositive(const std::string &value);

bool IsStringBoolNegative(const std::string &value);

bool GetStringBool(const std::string &value, bool &fOut);

bool IsStrOnlyDigits(const std::string &s);

std::string AmountToString(CAmount nValue);

std::string &TrimQuotes(std::string &s);

std::string &LTrimWhitespace(std::string &s);

std::string &RTrimWhitespace(std::string &s);

std::string &TrimWhitespace(std::string &s);

} // namespace str

} // namespace util

#endif //UNIT_E_UTILSTR_H
