// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>
#include <boost/test/unit_test_log.hpp>
#include <test/test_bitcoin.h>
#include <ufp64.h>
#include <util.h>

using namespace ufp64;

BOOST_FIXTURE_TEST_SUITE(ufp64_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(to_str_test)
{
    BOOST_CHECK_EQUAL("1.23456789", to_str(123456789));
    BOOST_CHECK_EQUAL("0.00001234", to_str(1234));
    BOOST_CHECK_EQUAL("456.7", to_str(45670000000));
    BOOST_CHECK_EQUAL("0.00000001", to_str(1));
    BOOST_CHECK_EQUAL("100", to_str(10000000000));
}

BOOST_AUTO_TEST_CASE(div_2uint_test)
{
    BOOST_CHECK_EQUAL("0.75", to_str(div_2uint(3, 4)));
    BOOST_CHECK_EQUAL("0.33333333", to_str(div_2uint(1, 3)));
    BOOST_CHECK_EQUAL("0.28571428", to_str(div_2uint(12, 42)));
    BOOST_CHECK_EQUAL("31.69230769", to_str(div_2uint(412, 13)));
}

BOOST_AUTO_TEST_CASE(add_uint_test)
{
    ufp64_t x = div_2uint(3, 4); //0.75
    BOOST_CHECK_EQUAL("12.75", to_str(add_uint(x, 12)));

    x = div_2uint(1, 3); //0.33333333
    BOOST_CHECK_EQUAL("12.33333333", to_str(add_uint(x, 12)));

    x = div_2uint(14, 3); //4,66666666
    BOOST_CHECK_EQUAL("4.66666666", to_str(add_uint(x, 0)));
}

BOOST_AUTO_TEST_CASE(mul_test)
{
    ufp64_t x = div_2uint(1, 3);  //0.33333333
    ufp64_t y = div_2uint(14, 3); //4.66666666

    BOOST_CHECK_EQUAL("1.55555553", to_str(mul(x, y)));
    BOOST_CHECK_EQUAL("0.1111111", to_str(mul(x, x)));
    BOOST_CHECK_EQUAL("21.77777771", to_str(mul(y, y)));
    BOOST_CHECK_EQUAL("100000000000", to_str(mul(to_ufp64(1), to_ufp64(100000000000)))); //10E11
}

BOOST_AUTO_TEST_CASE(mul_by_uint_test)
{
    ufp64_t x = div_2uint(1, 3);  //0.33333333
    ufp64_t y = div_2uint(14, 3); //4.66666666

    BOOST_CHECK_EQUAL("0.66666666", to_str(mul_by_uint(x, 2)));
    BOOST_CHECK_EQUAL("27.99999996", to_str(mul_by_uint(y, 6)));
    BOOST_CHECK_EQUAL("100000000000", to_str(mul_by_uint(to_ufp64(1), 100000000000))); //10E11
}

BOOST_AUTO_TEST_CASE(mul_to_uint_test)
{
    ufp64_t x = div_2uint(4, 3);  //1.33333333
    ufp64_t y = div_2uint(14, 3); //4.66666666

    BOOST_CHECK_EQUAL(2, mul_to_uint(x, 2));
    BOOST_CHECK_EQUAL(27, mul_to_uint(y, 6));
    BOOST_CHECK_EQUAL(std::numeric_limits<uint64_t>::max(), mul_to_uint(to_ufp64(1), std::numeric_limits<uint64_t>::max()));
}


BOOST_AUTO_TEST_CASE(div_by_uint_test)
{
    ufp64_t x = div_2uint(1, 3); //0.33333333
    BOOST_CHECK_EQUAL("0.11111111", to_str(div_by_uint(x, 3)));

    x = div_2uint(14, 6); //2.33333333
    BOOST_CHECK_EQUAL("1.16666666", to_str(div_by_uint(x, 2)));
}

BOOST_AUTO_TEST_CASE(div_uint_test)
{
    ufp64_t x = div_2uint(1, 3); //0.33333333
    BOOST_CHECK_EQUAL("3.00000003", to_str(div_uint(1, x)));

    x = div_2uint(14, 6); //2.33333333
    BOOST_CHECK_EQUAL("6", to_str(div_uint(14, x)));
}

BOOST_AUTO_TEST_CASE(div_to_uint_test)
{
    ufp64_t x = div_2uint(1, 3); //0.33333333
    BOOST_CHECK_EQUAL(3, div_to_uint(1, x));

    x = div_2uint(14, 6); //2.33333333
    BOOST_CHECK_EQUAL(6, div_to_uint(14, x));

    BOOST_CHECK_EQUAL(std::numeric_limits<uint64_t>::max(), div_to_uint(std::numeric_limits<uint64_t>::max(), to_ufp64(1)));
}

BOOST_AUTO_TEST_CASE(div_test)
{
    ufp64_t x = div_2uint(1, 3);
    ufp64_t y = div_2uint(14, 3);
    BOOST_CHECK_EQUAL("14.00000012", to_str(div(y, x)));
    BOOST_CHECK_EQUAL("1", to_str(div(x, x)));
    BOOST_CHECK_EQUAL("1", to_str(div(y, y)));
}

BOOST_AUTO_TEST_CASE(to_uint_test)
{
    BOOST_CHECK_EQUAL(0, to_uint(div_2uint(1, 3)));
    BOOST_CHECK_EQUAL(4, to_uint(div_2uint(14, 3)));
    BOOST_CHECK_EQUAL(333333, to_uint(div_2uint(1000000, 3)));
}

BOOST_AUTO_TEST_CASE(min_test)
{
    ufp64_t x = div_2uint(1, 3);
    ufp64_t y = div_2uint(14, 3);
    ufp64_t w = div_2uint(1, 6);
    ufp64_t zero = div_2uint(0, 1);
    BOOST_CHECK_EQUAL(x, min(x, y));
    BOOST_CHECK_EQUAL(x, min(y, x));
    BOOST_CHECK_EQUAL(w, min(x, w));
    BOOST_CHECK_EQUAL(zero, min(x, zero));
}

BOOST_AUTO_TEST_CASE(max_test)
{
    ufp64_t x = div_2uint(1, 3);
    ufp64_t y = div_2uint(14, 3);
    ufp64_t w = div_2uint(1, 6);
    ufp64_t zero = div_2uint(0, 1);
    BOOST_CHECK_EQUAL(y, max(x, y));
    BOOST_CHECK_EQUAL(y, max(y, x));
    BOOST_CHECK_EQUAL(x, max(x, w));
    BOOST_CHECK_EQUAL(x, max(x, zero));
}

BOOST_AUTO_TEST_CASE(add_test)
{
    ufp64_t x = div_2uint(1, 3);
    ufp64_t y = div_2uint(14, 3);
    BOOST_CHECK_EQUAL("4.99999999", to_str(add(x, y)));
}

BOOST_AUTO_TEST_CASE(sub_test)
{
    ufp64_t x = div_2uint(1, 3);
    ufp64_t y = div_2uint(14, 3);
    BOOST_CHECK_EQUAL("4.33333333", to_str(sub(y, x)));
}

BOOST_AUTO_TEST_CASE(sqrt_uint_test)
{
    BOOST_CHECK_EQUAL("12", to_str(sqrt_uint(144)));
    BOOST_CHECK_EQUAL("1.41421356", to_str(sqrt_uint(2)));
    BOOST_CHECK_EQUAL("1000000000", to_str(sqrt_uint(1000000000000000000)));
}

BOOST_AUTO_TEST_SUITE_END()
