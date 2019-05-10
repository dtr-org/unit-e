// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/txtools.h>

#include <script/script.h>
#include <script/sign.h>
#include <script/standard.h>

#include <boost/test/unit_test.hpp>

namespace txtools {

CKey TxTool::CreateKey() {
  CKey key;
  key.MakeNewKey(/* fCompressed= */ true);
  m_key_store.AddKey(key);
  return key;
}

CTransaction TxTool::CreateTransaction() {
  CMutableTransaction mtx;

  {
    const CKey key = CreateKey();
    const CAmount amount(1000);
    const CPubKey pub_key = key.GetPubKey();
    const CTxDestination destination = WitnessV0KeyHash(pub_key.GetID());
    const CTxOut out(amount, GetScriptForDestination(destination));

    CScript script_sig;
    mtx.vin.emplace_back(uint256::zero, 0, script_sig);

    SignatureData sigdata;
    const MutableTransactionSignatureCreator sigcreator(&mtx, 0, out.nValue, SIGHASH_ALL);
    BOOST_REQUIRE(ProduceSignature(m_key_store, sigcreator, out.scriptPubKey, sigdata));
    UpdateInput(mtx.vin.at(0), sigdata);
  }

  {
    const CKey key = CreateKey();
    const CPubKey pub_key = key.GetPubKey();
    const CTxDestination destination = WitnessV0KeyHash(pub_key.GetID());
    const CScript script_pub_key = GetScriptForDestination(destination);
    mtx.vout.emplace_back(CAmount(1), script_pub_key);
  }

  return CTransaction(mtx);
}

}

