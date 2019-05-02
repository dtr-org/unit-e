// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proposer/multiwallet.h>

#include <util.h>
#include <wallet/wallet.h>

namespace proposer {

class MultiWalletAdapter : public MultiWallet {

 public:
  const std::vector<std::shared_ptr<CWallet>> GetWallets() const override {
    return ::GetWallets();
  }
};

std::unique_ptr<MultiWallet> MultiWallet::New() {
  return MakeUnique<MultiWalletAdapter>();
}

}  // namespace proposer
