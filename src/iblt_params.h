// Copyright (c) 2019 The Unit-e developers
// Copyright (c) 2018 The Bitcoin Unlimited developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_IBLT_PARAMS_H
#define UNITE_IBLT_PARAMS_H

#include <vector>

class IbltParams {
 public:
  //! \brief Optimal iblt overhead
  // If IBLT would contain N items, than it would require N * overhead entries
  // in its table to efficiently decode data
  const float overhead;

  //! \brief Optimal number of hash functions to use
  const uint8_t num_hashes;

  IbltParams(float overhead, uint8_t num_hashes);

  static IbltParams FindOptimal(size_t expected_items_count);

 private:
  static std::vector<IbltParams> m_experimental_params;
};

#endif /* UNITE_IBLT_PARAMS_H */
