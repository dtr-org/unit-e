// Copyright (c) 2018 The Particl Core developers
// Copyright (c) 2019 The UnitE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <extkey.h>
#include <usbdevice/debugdevice.h>
#include <usbdevice/usbdevice.h>
#include <wallet/wallet.h>

#include <memory>
#include <vector>

namespace usbdevice {

const DeviceType usbDeviceTypes[] = {
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
    // Should we allow the user to pick one?
    error = "Multiple devices found.";
    return nullptr;
  }

  return devices[0];
}

DeviceSignatureCreator::DeviceSignatureCreator(
    std::shared_ptr<USBDevice> device, const CTransaction &tx, unsigned int nin,
    const CAmount &amount, int hash_type)
    : m_tx(tx),
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

  try {
    // Can we avoid tight coupling to CWallet here?
    const CWallet &wallet = dynamic_cast<const CWallet &>(provider);
    LOCK(wallet.cs_wallet);

    auto it = wallet.mapKeyMetadata.find(keyid);
    if (it == wallet.mapKeyMetadata.end()) {
      return false;
    }
    metadata = it->second;
  } catch (std::bad_cast &exp) {
    return false;
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
