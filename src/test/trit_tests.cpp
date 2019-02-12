// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <trit.h>
#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(trit_tests)

BOOST_AUTO_TEST_CASE(trit_unknown)
{
  // default initialization is unknown
  Trit trit;

  BOOST_CHECK(trit.IsUnknown());
  BOOST_CHECK(!trit.IsTrue());
  BOOST_CHECK(!trit.IsFalse());
}

BOOST_AUTO_TEST_CASE(trit_true)
{
  Trit trit = Trit(true);

  BOOST_CHECK(!trit.IsUnknown());
  BOOST_CHECK(trit.IsTrue());
  BOOST_CHECK(!trit.IsFalse());
}

BOOST_AUTO_TEST_CASE(trit_false)
{
  Trit trit = Trit(false);

  BOOST_CHECK(!trit.IsUnknown());
  BOOST_CHECK(!trit.IsTrue());
  BOOST_CHECK(trit.IsFalse());
}

BOOST_AUTO_TEST_CASE(trit_multi_and)
{
  {
    // All true yields true
    Trit t1 = Trit(true);
    Trit t2 = Trit(true);
    Trit t3 = Trit(true);
    Trit t4 = Trit(true);
    Trit t5 = Trit(true);

    BOOST_CHECK(Trit::And(t1, t2, t3, t4, t5).IsTrue());
  }
  {
    // A single false (among only trues) makes the result false
    Trit t1 = Trit(true);
    Trit t2 = Trit(true);
    Trit t3 = Trit(false);
    Trit t4 = Trit(true);
    Trit t5 = Trit(true);

    BOOST_CHECK(Trit::And(t1, t2, t3, t4, t5).IsFalse());
  }
  {
    // A single false (also among only unknowns) makes the result false
    Trit t1 = Trit();
    Trit t2 = Trit();
    Trit t3 = Trit(false);
    Trit t4 = Trit();
    Trit t5 = Trit();

    BOOST_CHECK(Trit::And(t1, t2, t3, t4, t5).IsFalse());
  }
  {
    // A single true (among only unknowns) leaves the result unknown
    Trit t1 = Trit();
    Trit t2 = Trit();
    Trit t3 = Trit(true);
    Trit t4 = Trit();
    Trit t5 = Trit();

    BOOST_CHECK(Trit::And(t1, t2, t3, t4, t5).IsUnknown());
  }
}

BOOST_AUTO_TEST_CASE(trit_multi_or)
{
  {
    // All true yields true
    Trit t1 = Trit(true);
    Trit t2 = Trit(true);
    Trit t3 = Trit(true);
    Trit t4 = Trit(true);
    Trit t5 = Trit(true);

    BOOST_CHECK(Trit::Or(t1, t2, t3, t4, t5).IsTrue());
  }
  {
    // A single false (among only trues) leaves the result still true
    Trit t1 = Trit(true);
    Trit t2 = Trit(true);
    Trit t3 = Trit(false);
    Trit t4 = Trit(true);
    Trit t5 = Trit(true);

    BOOST_CHECK(Trit::Or(t1, t2, t3, t4, t5).IsTrue());
  }
  {
    // A single false (also among only unknowns) makes the result unknown
    Trit t1 = Trit();
    Trit t2 = Trit();
    Trit t3 = Trit(false);
    Trit t4 = Trit();
    Trit t5 = Trit();

    BOOST_CHECK(Trit::Or(t1, t2, t3, t4, t5).IsUnknown());
  }
  {
    // A single true (among only unknowns) leaves the result true
    Trit t1 = Trit();
    Trit t2 = Trit();
    Trit t3 = Trit(true);
    Trit t4 = Trit();
    Trit t5 = Trit();

    BOOST_CHECK(Trit::Or(t1, t2, t3, t4, t5).IsTrue());
  }
}

BOOST_AUTO_TEST_SUITE_END()
