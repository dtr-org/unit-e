// Copyright (c) 2019 The Unit-e developers
// Copyright (c) 2018 The Unit-e Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <iblt_params.h>

IBLTParams::IBLTParams(const float overhead, const uint8_t num_hashes)
    : overhead(overhead), num_hashes(num_hashes) {}

// See https://github.com/umass-forensics/IBLT-optimization
// for how this was generated
// Maps expected_items_count -> optimal params, where expected_items_count is
// index in the vector
IBLTParams IBLTParams::experimental_params[] = {
#include <iblt_params.table>
};

IBLTParams IBLTParams::FindOptimal(const size_t expected_items_count) {
  constexpr size_t PARAMS_LENGTH =
      sizeof(experimental_params) / sizeof(experimental_params[0]);

  if (expected_items_count >= PARAMS_LENGTH) {
    return experimental_params[0];
  }

  return experimental_params[expected_items_count];
}
