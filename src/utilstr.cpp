// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "utilstr.h"

namespace util
{
namespace str
{

static bool icompare_pred(unsigned char a, unsigned char b)
{
    return std::tolower(a) == std::tolower(b);
}

static bool icompare_str(const std::string &a, const std::string &b)
{
    return a.length() == b.length()
           && std::equal(b.begin(), b.end(), a.begin(), icompare_pred);
}

void ReplaceStrInPlace(std::string &subject, const std::string search, const std::string replace)
{
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos)
    {
        subject.replace(pos, search.length(), replace);
        pos += replace.length();
    }
}

bool IsStringBoolPositive(const std::string &value)
{
    return (value == "+" || value == "1" || icompare_str(value, "on")  || icompare_str(value, "true") || icompare_str(value, "yes") || icompare_str(value, "y"));
}

bool IsStringBoolNegative(const std::string &value)
{
    return (value == "-" || value == "0" || icompare_str(value, "off") || icompare_str(value, "false") || icompare_str(value, "no") || icompare_str(value, "n"));
}

bool GetStringBool(const std::string &value, bool &fOut)
{
    if (IsStringBoolPositive(value))
    {
        fOut = true;
        return true;
    }

    if (IsStringBoolNegative(value))
    {
        fOut = false;
        return true;
    }

    return false;
}

bool IsStrOnlyDigits(const std::string &s)
{
    return s.find_first_not_of("0123456789") == std::string::npos;
}

std::string AmountToString(CAmount nValue)
{
    bool sign = nValue < 0;
    int64_t n_abs = (sign ? -nValue : nValue);
    int64_t quotient = n_abs / UNIT;
    int64_t remainder = n_abs % UNIT;
    return strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder);
}

std::string &TrimQuotes(std::string &s)
{
    if (s.size() < 1) {
        return s;
    }
    if (s.front() == '"') {
        s.erase(0, 1);
    }
    size_t n = s.size();
    if (n < 1) {
        return s;
    }
    if (n > 1 && s[n-2] == '\\') { // don't strip \"
        return s;
    }
    if (s.back() == '"') {
        s.erase(n - 1);
    }
    return s;
}

std::string &TrimWhitespace(std::string &s)
{
    LTrimWhitespace(s);
    RTrimWhitespace(s);
    return s;
}

std::string &LTrimWhitespace(std::string &s)
{
    std::string::iterator i;
    for (i = s.begin(); i != s.end(); ++i)
        if (!std::isspace(*i))
            break;
    if (i != s.begin())
        s.erase(s.begin(), i);
    return s;
}

std::string &RTrimWhitespace(std::string &s)
{
    std::string::reverse_iterator i;
    for (i = s.rbegin(); i != s.rend(); ++i)
        if (!std::isspace(*i))
            break;
    if (i != s.rbegin())
        s.erase(i.base(), s.end());
    return s;
}


} // namespace str

} // namespace util
