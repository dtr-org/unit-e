// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2018-2019 The Unit-e developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <extkey.h>
#include <usbdevice/debugdevice.h>
#include <usbdevice/usbdevice.h>
#include <wallet/wallet.h>

#include <memory>
#include <vector>

namespace usbdevice {

const DeviceType USB_DEVICE_TYPES[] = {
    DeviceType(0x0000, 0x0000, "Debug", "Device", USBDEVICE_DEBUG),
};

bool ListAllDevices(DeviceList &devices) {
  devices.push_back(std::shared_ptr<USBDevice>(new DebugDevice()));
  return true;
}

std::shared_ptr<USBDevice> SelectDevice(std::string &error) {
  DeviceList devices;
  ListAllDevices(devices);
  if (devices.size() < 1) {
    error = "No device found.";
    return nullptr;
  }
  if (devices.size() > 1) {
    // UNIT-E TODO: Allow the user to pick one in the UI
    error = "Multiple devices found.";
    return nullptr;
  }

  return devices[0];
}

DeviceSignatureCreator::DeviceSignatureCreator(
    std::shared_ptr<USBDevice> device, const CWallet &wallet,
    const CTransaction &tx, unsigned int nin, const CAmount &amount,
    int hash_type)
    : m_wallet(wallet),
      m_tx(tx),
      m_nin(nin),
      m_hash_type(hash_type),
      m_amount(amount),
      m_device(device),
      m_checker(&tx, nin, amount) {}

bool DeviceSignatureCreator::CreateSig(const SigningProvider &provider,
                                       std::vector<unsigned char> &signature,
                                       const CKeyID &keyid,
                                       const CScript &script_code,
                                       SigVersion sigversion) const {
  CKeyMetadata metadata;

  {
    LOCK(m_wallet.cs_wallet);

    auto it = m_wallet.mapKeyMetadata.find(keyid);
    if (it == m_wallet.mapKeyMetadata.end()) {
      return false;
    }
    metadata = it->second;
  }

  std::vector<uint32_t> path;
  std::string error;
  if (!ParseExtKeyPath(metadata.hdKeypath, path, error)) {
    return false;
  }

  return m_device->SignTransaction(path, m_tx, m_nin, script_code, m_hash_type,
                                   m_amount, sigversion, signature, error);
}

}  // namespace usbdevice
