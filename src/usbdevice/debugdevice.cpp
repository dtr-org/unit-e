// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <hash.h>
#include <key.h>
#include <univalue.h>
#include <usbdevice/debugdevice.h>
#include <utilstrencodings.h>
#include <validation.h>

#include <memory>
#include <vector>

namespace usbdevice {

static const uint8_t seed[] = "debug key";

//! Maximum supported depth for BIP32-derived keys
const size_t MAX_BIP32_PATH = 10;

DebugDevice::DebugDevice()
    : USBDevice(&usbDeviceTypes[USBDEVICE_DEBUG - 1], "none", "1", 0) {
  m_ekv.SetSeed(seed, sizeof(seed) - 1);  // to account for trailing \0
}

bool DebugDevice::Open() { return true; }

bool DebugDevice::Close() { return true; }

bool DebugDevice::GetFirmwareVersion(std::string &firmware, std::string &error) {
  firmware = "debug v1";
  return true;
}

bool DebugDevice::GetPubKey(
    const std::vector<uint32_t> &path, CPubKey &pk, std::string &error) {
  CExtPubKey epk;
  if (!GetExtPubKey(path, epk, error)) {
    return false;
  }

  pk = epk.pubkey;
  return true;
}

bool DebugDevice::GetExtPubKey(
    const std::vector<uint32_t> &path, CExtPubKey &epk, std::string &error) {
  if (path.size() > MAX_BIP32_PATH) {
    error = "Path depth out of range";
    return false;
  }

  CExtKey keyOut, keyWork = m_ekv;
  for (auto it = path.begin(); it != path.end(); ++it) {
    if (!keyWork.Derive(keyOut, *it)) {
      error = "CExtKey derive failed";
      return false;
    }
    keyWork = keyOut;
  }
  epk = keyWork.Neuter();

  return true;
}

bool DebugDevice::PrepareTransaction(
    const CTransaction &tx, const CCoinsViewCache &view,
    const CKeyStore &keystore, int hash_type, std::string &error) {
  return true;
}

bool DebugDevice::SignTransaction(
    const std::vector<uint32_t> &path, const std::vector<uint8_t> &tweak,
    const CTransaction &tx, int n_in, const CScript &script_code,
    int hash_type, const CAmount &amount, SigVersion sigversion,
    std::vector<uint8_t> &signature, std::string &error) {
  uint256 hash = SignatureHash(script_code, tx, n_in, hash_type, amount, sigversion);

  CExtKey keyOut, keyWork = m_ekv;
  for (auto it = path.begin(); it != path.end(); ++it) {
    if (!keyWork.Derive(keyOut, *it)) {
      error = "CExtKey derive failed";
      return false;
    }
    keyWork = keyOut;
  }

  CKey key = keyWork.key;
  if (tweak.size() == 32) {
    std::vector<uint8_t> keydata(key.begin(), key.end());
    secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    int add_result = secp256k1_ec_privkey_tweak_add(
        ctx, keydata.data(), tweak.data());
    secp256k1_context_destroy(ctx);

    if (!add_result) {
      error = "Add failed";
      return false;
    }

    key.Set(keydata.begin(), keydata.end(), false);
  }

  if (!key.Sign(hash, signature)) {
    error = "Sign failed";
    return false;
  }
  signature.push_back((unsigned char)hash_type);

  return true;
}

}  // namespace usbdevice
