// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>

#include <util.h>
#include <wallet/wallet.h>

namespace proposer {

class MultiWalletAdapter : public MultiWallet {

 public:
  const std::vector<CWallet *> &GetWallets() const override {
    return vpwallets;
  }
};

std::unique_ptr<MultiWallet> MultiWallet::MakeMultiWallet() {
  return MakeUnique<MultiWalletAdapter>();
}

}  // namespace proposer
