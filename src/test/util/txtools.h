// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNIT_E_TEST_UTIL_TXTOOLS_H
#define UNIT_E_TEST_UTIL_TXTOOLS_H

#include <key.h>
#include <keystore.h>
#include <primitives/transaction.h>

namespace txtools {

class TxTool {

 private:
  CBasicKeyStore m_key_store;

 public:
  CKey CreateKey();

  //! \brief Creates a mocked, but properly signed, P2WPKH transaction.
  CTransaction CreateTransaction();
};

};

#endif //UNIT_E_TEST_UTIL_TXTOOLS_H
