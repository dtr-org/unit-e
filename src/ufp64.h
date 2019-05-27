// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_UFP32_H
#define UNIT_E_UFP32_H

#include <arith_uint256.h>
#include <stdint.h>
#include <string>

namespace ufp64
{
typedef uint64_t ufp64_t;

ufp64_t mul(ufp64_t x, ufp64_t y);

ufp64_t mul_by_uint(ufp64_t x, uint64_t y);

uint64_t mul_to_uint(ufp64_t x, uint64_t y);

ufp64_t div(ufp64_t x, ufp64_t y);

ufp64_t div_2uint(uint64_t x, uint64_t y);

ufp64_t div_by_uint(ufp64_t x, uint64_t y);

ufp64_t div_uint(uint64_t y, ufp64_t x);

uint64_t div_to_uint(uint64_t y, ufp64_t x);

ufp64_t add_uint(ufp64_t ufp, uint32_t uint);

ufp64_t add(ufp64_t x, ufp64_t y);

ufp64_t sub(ufp64_t x, ufp64_t y);

ufp64_t min(ufp64_t x, ufp64_t y);

ufp64_t max(ufp64_t x, ufp64_t y);

ufp64_t sqrt_uint(uint64_t x);

uint64_t to_uint(ufp64_t x);

ufp64_t to_ufp64(uint64_t x);

std::string to_str(uint64_t x);
} // namespace ufp64

#endif //UNIT_E_UFP32_H
