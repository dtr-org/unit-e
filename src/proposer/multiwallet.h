// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_PROPOSER_MULTIWALLET_H
#define UNIT_E_PROPOSER_MULTIWALLET_H

#include <memory>
#include <vector>

class CWallet;

namespace proposer {

class MultiWallet {

 public:
  virtual const std::vector<CWallet *> GetWallets() const = 0;

  virtual ~MultiWallet() = default;

  static std::unique_ptr<MultiWallet> MakeMultiWallet();
};

}  // namespace proposer

#endif  // UNIT_E_PROPOSER_MULTIWALLET_H
