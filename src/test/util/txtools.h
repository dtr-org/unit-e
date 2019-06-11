// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TEST_UTIL_TXTOOLS_H
#define UNITE_TEST_UTIL_TXTOOLS_H

#include <key.h>
#include <keystore.h>
#include <primitives/transaction.h>

namespace txtools {

class TxTool {

 public:
  CKey CreateKey();

  //! \brief Creates a mocked, but properly signed, P2WPKH transaction.
  CTransaction CreateTransaction();

 private:
  CBasicKeyStore m_key_store;

  void AddSomeOutput(CMutableTransaction& mtx, CAmount amount = 1);

};

};

#endif // UNITE_TEST_UTIL_TXTOOLS_H
