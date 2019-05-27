// Copyright (c) 2012-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/strencodings.h>
#include <util/system.h>
#include <test/test_unite.h>

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

namespace {
struct Fixture {
  ::ArgsManager args;
  void ResetArgs(const std::string& strArg)
  {
    std::vector<std::string> vecArg;
    if (strArg.size())
      boost::split(vecArg, strArg, IsSpace, boost::token_compress_on);

    // Insert dummy executable name:
    vecArg.insert(vecArg.begin(), "testunite");

    // Convert to char*:
    std::vector<const char*> vecChar;
    for (const std::string& s : vecArg)
      vecChar.push_back(s.c_str());

    std::string error;
    BOOST_CHECK(args.ParseParameters(vecChar.size(), vecChar.data(), error));
}

  void SetupArgs(const std::vector<std::string>& arg_names)
  {
    args.ClearArgs();
    for (const std::string& arg : arg_names) {
      args.AddArg(arg, "", false, OptionsCategory::OPTIONS);
    }
  }
};
}


BOOST_FIXTURE_TEST_SUITE(getarg_tests, Fixture)
BOOST_AUTO_TEST_CASE(boolarg)
{
    SetupArgs({"-foo"});
    ResetArgs("-foo");
    BOOST_CHECK(args.GetBoolArg("-foo", false));
    BOOST_CHECK(args.GetBoolArg("-foo", true));

    BOOST_CHECK(!args.GetBoolArg("-fo", false));
    BOOST_CHECK(args.GetBoolArg("-fo", true));

    BOOST_CHECK(!args.GetBoolArg("-fooo", false));
    BOOST_CHECK(args.GetBoolArg("-fooo", true));

    ResetArgs("-foo=0");
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

    ResetArgs("-foo=1");
    BOOST_CHECK(args.GetBoolArg("-foo", false));
    BOOST_CHECK(args.GetBoolArg("-foo", true));

    // New 0.6 feature: auto-map -nosomething to !-something:
    ResetArgs("-nofoo");
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

    ResetArgs("-nofoo=1");
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

    ResetArgs("-foo -nofoo");  // -nofoo should win
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

    ResetArgs("-foo=1 -nofoo=1");  // -nofoo should win
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

    ResetArgs("-foo=0 -nofoo=0");  // -nofoo=0 should win
    BOOST_CHECK(args.GetBoolArg("-foo", false));
    BOOST_CHECK(args.GetBoolArg("-foo", true));

    // New 0.6 feature: treat -- same as -:
    ResetArgs("--foo=1");
    BOOST_CHECK(args.GetBoolArg("-foo", false));
    BOOST_CHECK(args.GetBoolArg("-foo", true));

    ResetArgs("--nofoo=1");
    BOOST_CHECK(!args.GetBoolArg("-foo", false));
    BOOST_CHECK(!args.GetBoolArg("-foo", true));

}

BOOST_AUTO_TEST_CASE(stringarg)
{
    SetupArgs({"-foo", "-bar"});
    ResetArgs("");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", "eleven"), "eleven");

    ResetArgs("-foo -bar");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", "eleven"), "");

    ResetArgs("-foo=");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", "eleven"), "");

    ResetArgs("-foo=11");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "11");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", "eleven"), "11");

    ResetArgs("-foo=eleven");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "eleven");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", "eleven"), "eleven");

}

BOOST_AUTO_TEST_CASE(intarg)
{
    SetupArgs({"-foo", "-bar"});
    ResetArgs("");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", 11), 11);
    BOOST_CHECK_EQUAL(args.GetArg("-foo", 0), 0);

    ResetArgs("-foo -bar");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", 11), 0);
    BOOST_CHECK_EQUAL(args.GetArg("-bar", 11), 0);

    ResetArgs("-foo=11 -bar=12");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", 0), 11);
    BOOST_CHECK_EQUAL(args.GetArg("-bar", 11), 12);

    ResetArgs("-foo=NaN -bar=NotANumber");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", 1), 0);
    BOOST_CHECK_EQUAL(args.GetArg("-bar", 11), 0);
}

BOOST_AUTO_TEST_CASE(doubledash)
{
    SetupArgs({"-foo", "-bar"});
    ResetArgs("--foo");
    BOOST_CHECK_EQUAL(args.GetBoolArg("-foo", false), true);

    ResetArgs("--foo=verbose --bar=1");
    BOOST_CHECK_EQUAL(args.GetArg("-foo", ""), "verbose");
    BOOST_CHECK_EQUAL(args.GetArg("-bar", 0), 1);
}

BOOST_AUTO_TEST_CASE(boolargno)
{
    SetupArgs({"-foo", "-bar"});
    ResetArgs("-nofoo");
    BOOST_CHECK(!args.GetBoolArg("-foo", true));
    BOOST_CHECK(!args.GetBoolArg("-foo", false));

    ResetArgs("-nofoo=1");
    BOOST_CHECK(!args.GetBoolArg("-foo", true));
    BOOST_CHECK(!args.GetBoolArg("-foo", false));

    ResetArgs("-nofoo=0");
    BOOST_CHECK(args.GetBoolArg("-foo", true));
    BOOST_CHECK(args.GetBoolArg("-foo", false));

    ResetArgs("-foo --nofoo"); // --nofoo should win
    BOOST_CHECK(!args.GetBoolArg("-foo", true));
    BOOST_CHECK(!args.GetBoolArg("-foo", false));

    ResetArgs("-nofoo -foo"); // foo always wins:
    BOOST_CHECK(args.GetBoolArg("-foo", true));
    BOOST_CHECK(args.GetBoolArg("-foo", false));
}

BOOST_AUTO_TEST_SUITE_END()
