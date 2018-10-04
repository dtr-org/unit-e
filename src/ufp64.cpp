// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <ufp64.h>

static const int SCALE = 100000000;

/**
 * This namespace implements support for fixed-point arithmetics as ufp64 is a fixed-point number represented with 64
 * bits. Be aware that the precision for the decimal part is 10E8 and the integer part up 10E11. While all internal and
 * intermediate operation results make use of a uint256, the result can overflow nonetheless.
 */
namespace ufp64
{
static const int SQRT_ITERATIONS = 60;

ufp64_t add_uint(ufp64_t ufp, uint32_t uint)
{
    return (uint * SCALE) + ufp;
}

ufp64_t mul(ufp64_t x, ufp64_t y)
{
    arith_uint256 x1(x);
    arith_uint256 y1(y);
    return ((x1 * y1) / SCALE).GetLow64();
}

ufp64_t mul_by_uint(ufp64_t x, uint64_t y)
{
    arith_uint256 x1(x);
    arith_uint256 y1(y);
    return (x1 * y1).GetLow64();
}

uint64_t mul_to_uint(ufp64_t x, uint64_t y)
{
    arith_uint256 x1(x);
    arith_uint256 y1(y);
    return ((x1 * y1) / SCALE).GetLow64();
}

ufp64_t div_2uint(uint64_t x, uint64_t y)
{
    arith_uint256 x1(x);
    return ((x1 * SCALE) / y).GetLow64();
}

ufp64_t div_by_uint(ufp64_t x, uint64_t y)
{
    arith_uint256 x1(x);
    return (x1 / y).GetLow64();
}

ufp64_t div_uint(uint64_t x, ufp64_t y)
{
    arith_uint256 x1(x);
    return (x1 * SCALE * SCALE / y).GetLow64();
}

ufp64_t div_to_uint(uint64_t x, ufp64_t y)
{
    arith_uint256 x1(x);
    return (x1 * SCALE / y).GetLow64();
}

ufp64_t div(ufp64_t x, ufp64_t y)
{
    arith_uint256 x1(x);
    return (x1 * SCALE / y).GetLow64();
}

ufp64_t add(ufp64_t x, ufp64_t y)
{
    return x + y;
}

ufp64_t sub(ufp64_t x, ufp64_t y)
{
    return x - y;
}

ufp64_t min(ufp64_t x, ufp64_t y)
{
    return std::min(x, y);
}

ufp64_t max(ufp64_t x, ufp64_t y)
{
    return std::max(x, y);
}

ufp64_t sqrt_uint(uint64_t x)
{
    arith_uint256 y(x);
    y = y * SCALE * SCALE; //Since we are going to sqrt the input, we should scale it by SCALE^2

    //Using Babylonian algorithm to calculate the sqrt
    arith_uint256 sqrt = y / 2;
    for (int i = 0; i < SQRT_ITERATIONS; i++) {
        sqrt = (sqrt + (y / sqrt)) / 2;
    }
    return sqrt.GetLow64();
}

uint64_t to_uint(ufp64_t x)
{
    return x / SCALE;
}

//Be careful not to cause overflows here
ufp64_t to_ufp64(uint64_t x)
{
    return x * SCALE;
}

std::string to_str(ufp64_t x)
{
    std::string unscaled = std::to_string(x);

    if (unscaled.length() < 9) {
        unscaled.insert(0, 9 - unscaled.length(), '0');
    }

    size_t dotIndex = unscaled.length() - 8;
    std::string result = unscaled.substr(0, dotIndex) + "." + unscaled.substr(dotIndex, unscaled.length());

    while (result.length() > dotIndex && (result.back() == '0' || result.back() == '.')) {
        result.pop_back();
    }

    return result;
}
} // namespace ufp64
