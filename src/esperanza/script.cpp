// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <esperanza/script.h>

#include <script/standard.h>

namespace esperanza {

bool ExtractValidatorPubkey(const CTransaction &tx, CPubKey &pubkeyOut) {
  if (tx.IsVote()) {

    std::vector<std::vector<unsigned char>> vSolutions;

    if (Solver(tx.vout[0].scriptPubKey, vSolutions)) {
      pubkeyOut = CPubKey(vSolutions[0]);
      return true;
    }
  }
  return false;
}

bool ExtractValidatorAddress(const CTransaction &tx,
                             uint160 &validatorAddressOut) {

  switch (tx.GetType()) {
    case TxType::DEPOSIT:
    case TxType::LOGOUT: {
      std::vector<std::vector<unsigned char>> vSolutions;

      if (Solver(tx.vout[0].scriptPubKey, vSolutions)) {
        validatorAddressOut = CPubKey(vSolutions[0]).GetID();
        return true;
      }
      return false;
    }
    case TxType::WITHDRAW: {

      const CScript scriptSig = tx.vin[0].scriptSig;
      auto pc = scriptSig.begin();
      std::vector<unsigned char> vData;
      opcodetype opcode;

      // Skip the first value (signature)
      scriptSig.GetOp(pc, opcode);

      // Retrieve the public key
      scriptSig.GetOp(pc, opcode, vData);
      validatorAddressOut = CPubKey(vData).GetID();
      return true;
    }
    default: {
      return false;
    }
  }
}

}  // namespace esperanza
