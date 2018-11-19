// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/esperanza/finalizationstate_utils.h>

uint160 RandValidatorAddr() {
  CKey key;
  key.MakeNewKey(true);
  return key.GetPubKey().GetID();
}
