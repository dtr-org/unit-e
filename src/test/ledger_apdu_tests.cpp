// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <keystore.h>
#include <script/script.h>
#include <test/test_unite.h>
#include <usbdevice/ledgerapdu.h>

#include <cstdint>
#include <cstdlib>

#include <boost/test/unit_test.hpp>

namespace {

template <size_t Size>
std::vector<uint8_t> randvec() {
  std::vector<uint8_t> result(Size);
  for (size_t i = 0; i < Size; ++i) {
    result[i] = rand() % 256;
  }
  return result;
}

}  // namespace

const size_t APDU_HEADER_SIZE = 5;

BOOST_AUTO_TEST_CASE(apdu_test_ext_pubkey) {
  usbdevice::APDU apdu;
  std::string error;
  bool result;

  // path too long
  {
    std::vector<uint32_t> path = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    result = usbdevice::GetExtPubKeyAPDU(path, apdu, error);
    BOOST_CHECK(!result);
  }

  {
    std::vector<uint32_t> path = {1, 2, 3, 4};
    result = usbdevice::GetExtPubKeyAPDU(path, apdu, error);
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(apdu.size, 5 + 1 + 4 * path.size());
  }
}

BOOST_AUTO_TEST_CASE(apdu_test_prepare_segwit) {
  CCoinsView view;
  CCoinsViewCache view_cache(&view);
  std::vector<usbdevice::APDU> apdus;
  std::string error;
  bool result;

  COutPoint outpoint1(uint256(randvec<32>()), 0);
  CScript script1;
  script1 << OP_0;
  Coin coin1(CTxOut(100000, script1), 0, TxType::REGULAR);
  view_cache.AddCoin(outpoint1, std::move(coin1), false);

  COutPoint outpoint2(uint256(randvec<32>()), 0);
  CScript script2;
  script2 << randvec<1200>();
  Coin coin2(CTxOut(100000, script2), 0, TxType::REGULAR);

  // check that small transactions are broken up correctly
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint1));
    tx.vout.push_back(CTxOut(50000, CScript() << OP_0));

    result = usbdevice::GetPreparePhaseAPDUs(tx, view_cache, apdus, error);
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(apdus.size(), 3);

    BOOST_CHECK_EQUAL(apdus[0].size, APDU_HEADER_SIZE + 5);
    BOOST_CHECK_EQUAL(apdus[1].size, APDU_HEADER_SIZE + 1 + 36 + 8 + 1 + 4);
    BOOST_CHECK_EQUAL(apdus[2].size, APDU_HEADER_SIZE + 1 + 8 + 1 +
                                         tx.vout[0].scriptPubKey.size());
  }

  // check that large output scripts are split over multiple APDUs
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint1));
    tx.vout.push_back(CTxOut(50000, CScript() << randvec<1500>()));

    result = usbdevice::GetPreparePhaseAPDUs(tx, view_cache, apdus, error);
    BOOST_CHECK(result);

    size_t output_payload_size = 0;
    for (size_t i = 2; i < apdus.size(); i++) {
      output_payload_size += (apdus[i].size - APDU_HEADER_SIZE);
    }
    BOOST_CHECK_EQUAL(output_payload_size,
                      1 + 8 + 3 + tx.vout[0].scriptPubKey.size());
  }

  // check that the function fails if we try to spend a nonexistent coin
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint2));
    tx.vout.push_back(CTxOut(50000, CScript() << OP_0));

    result = usbdevice::GetPreparePhaseAPDUs(tx, view_cache, apdus, error);
    BOOST_CHECK(!result);
  }

  view_cache.AddCoin(outpoint2, std::move(coin2), false);

  // check that multiple inputs and outputs are ok
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint1));
    tx.vin.push_back(CTxIn(outpoint2));
    tx.vout.push_back(
        CTxOut(50000, CScript() << std::vector<uint8_t>(5, 0xAB)));
    tx.vout.push_back(
        CTxOut(50000, CScript() << std::vector<uint8_t>(300, 0xCD)));

    result = usbdevice::GetPreparePhaseAPDUs(tx, view_cache, apdus, error);
    BOOST_CHECK(result);

    size_t output_payload_size = 0;
    for (size_t i = 3; i < apdus.size(); i++) {
      output_payload_size += (apdus[i].size - APDU_HEADER_SIZE);
    }
    BOOST_CHECK_EQUAL(output_payload_size,
                      1 + 8 + 1 + tx.vout[0].scriptPubKey.size() + 8 + 3 +
                          tx.vout[1].scriptPubKey.size());
  }
}

BOOST_AUTO_TEST_CASE(apdu_test_sign_segwit) {
  CCoinsView view;
  CCoinsViewCache view_cache(&view);
  int hash_type = SIGHASH_ALL;
  std::vector<usbdevice::APDU> apdus;
  std::string error;
  bool result;
  std::vector<uint32_t> path{1, 2, 3, 4};

  COutPoint outpoint1(uint256(randvec<32>()), 0);
  CScript script1;
  script1 << OP_0;
  Coin coin1(CTxOut(100000, script1), 0, TxType::REGULAR);
  view_cache.AddCoin(outpoint1, std::move(coin1), false);

  COutPoint outpoint2(uint256(randvec<32>()), 0);
  CScript script2;
  script2 << randvec<1200>();
  Coin coin2(CTxOut(100000, script2), 0, TxType::REGULAR);

  // check that small transactions are broken up correctly
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint1));

    result =
        usbdevice::GetSignPhaseAPDUs(path, tx, 0, script1, hash_type, 50000,
                                     SigVersion::WITNESS_V0, apdus, error);
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(apdus.size(), 3);

    BOOST_CHECK_EQUAL(apdus[0].size, APDU_HEADER_SIZE + 5);
    BOOST_CHECK_EQUAL(apdus[1].size,
                      APDU_HEADER_SIZE + 1 + 36 + 8 + 1 + script1.size() + 4);
    BOOST_CHECK_EQUAL(apdus[2].size,
                      APDU_HEADER_SIZE + 1 + 4 * path.size() + 1 + 4 + 1);
  }

  // check that large scripts are split over multiple APDUs
  {
    CMutableTransaction tx;
    tx.vin.push_back(CTxIn(outpoint2));

    result =
        usbdevice::GetSignPhaseAPDUs(path, tx, 0, script2, hash_type, 50000,
                                     SigVersion::WITNESS_V0, apdus, error);
    BOOST_CHECK(result);
    BOOST_CHECK_EQUAL(apdus.rbegin()->size,
                      APDU_HEADER_SIZE + 1 + 4 * path.size() + 1 + 4 + 1);

    size_t input_payload_size = 0;
    for (size_t i = 1; i < apdus.size() - 1; i++) {
      input_payload_size += (apdus[i].size - APDU_HEADER_SIZE);
    }

    BOOST_CHECK_EQUAL(input_payload_size, 1 + 36 + 8 + 3 + script2.size() + 4);
  }
}
