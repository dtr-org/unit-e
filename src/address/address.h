// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNIT_E_ADDRESS_ADDRESS_H
#define UNIT_E_ADDRESS_ADDRESS_H

#include <string>
#include <base58.h>
#include <bech32.h>
#include <pubkey.h>

namespace address {

/*!
 * An address that can be used for sending or receiving money.
 */
class Address {

 public:
  Address(std::string &addressString);

  bool IsValid() const;

  bool GetKeyID(CKeyID& keyOut) const;

};

}

#endif // UNIT_E_ADDRESS_ADDRESS_H
