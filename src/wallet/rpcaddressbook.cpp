// Copyright (c) 2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/rpcaddressbook.h>

#include <rpc/safemode.h>
#include <rpc/server.h>
#include <univalue.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>

using AddressBookIter = std::map<CTxDestination, CAddressBookData>::iterator;

enum class MatchOwned { ALL = 0, ONLY_OWNED = 1, ONLY_NOT_OWNED = 2 };

static bool CompareCharsI(unsigned char a, unsigned char b) {
  return std::tolower(a) == std::tolower(b);
}

static bool StringCointainsI(const std::string &sString,
                             const std::string &sFind) {
  // UNIT-E: TODO: unicode
  return std::search(sString.begin(), sString.end(), sFind.begin(), sFind.end(),
                     CompareCharsI) != sString.end();
}

UniValue addressbookinfo(const JSONRPCRequest &request) {
  CWallet *pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() > 0) {
    throw std::runtime_error(
        "addressbookinfo\n"
        "Returns the number of entries in the address book\n");
  }

  ObserveSafeMode();

  UniValue result(UniValue::VOBJ);
  int numReceive = 0, numSend = 0;
  {
    LOCK(pwallet->cs_wallet);

    result.pushKV("total", static_cast<int>(pwallet->mapAddressBook.size()));

    for (const auto &pair : pwallet->mapAddressBook) {
      if (IsMine(*pwallet, pair.first)) {
        ++numReceive;
      } else {
        ++numSend;
      }
    }
  }

  result.pushKV("num_receive", numReceive);
  result.pushKV("num_send", numSend);
  return result;
}

UniValue filteraddresses(const JSONRPCRequest &request) {
  CWallet *pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() > 6) {
    throw std::runtime_error(
        "filteraddresses ( offset count sort_code \"search\" match_owned )\n"
        "\nList addresses.\n"
        "\nArguments:\n"
        "1. \"offset\":      (numeric, optional) number of addresses to skip\n"
        "2. \"count\":       (numeric, optional) number of addresses to be "
        "displayed\n"
        "3. \"sort_code\":   (numeric, optional) 0 sort by label ascending,\n"
        "                  1 sort by label descending, default 0\n"
        "4. \"search\":      (string, optional) a query to search labels\n"
        "5. \"match_owned\": (numeric, optional) 0 off, 1 owned, 2 non-owned,\n"
        "                  default 0\n");
  }

  ObserveSafeMode();

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  int offset = 0;
  int count = std::numeric_limits<int>::max();
  if (!request.params.empty()) {
    offset = request.params[0].get_int();
  }

  if (request.params.size() > 1) {
    count = request.params[1].get_int();
  }

  if (offset < 0) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "offset must be 0 or greater.");
  }
  if (count < 1) {
    throw JSONRPCError(RPC_INVALID_PARAMETER, "count must be 1 or greater.");
  }

  bool sortAsc = true;
  if (request.params.size() > 2) {
    int sortCode = request.params[2].get_int();
    if (sortCode != 0 && sortCode != 1) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown sort_code.");
    }
    sortAsc = sortCode == 0;
  }

  std::string search;
  if (request.params.size() > 3) {
    search = request.params[3].get_str();
  }

  MatchOwned matchOwned = MatchOwned::ALL;
  if (request.params.size() > 4) {
    int i = request.params[4].get_int();
    if (i < 0 || i > 2) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown match_owned.");
    }
    matchOwned = static_cast<MatchOwned>(i);
  }

  UniValue result(UniValue::VARR);
  {
    LOCK(pwallet->cs_wallet);

    if (offset && offset >= static_cast<int>(pwallet->mapAddressBook.size())) {
      throw JSONRPCError(
          RPC_INVALID_PARAMETER,
          strprintf("offset is beyond last address (%d).", offset));
    }

    std::vector<AddressBookIter> vitMapAddressBook;
    vitMapAddressBook.reserve(pwallet->mapAddressBook.size());
    std::map<CTxDestination, bool> addressIsMine;
    for (auto it = pwallet->mapAddressBook.begin();
         it != pwallet->mapAddressBook.end(); ++it) {
      addressIsMine[it->first] = !!IsMine(*pwallet, it->first);

      if (matchOwned == MatchOwned::ONLY_OWNED && !addressIsMine[it->first]) {
        continue;
      } else if (matchOwned == MatchOwned::ONLY_NOT_OWNED &&
                 addressIsMine[it->first]) {
        continue;
      }

      if (!search.empty() && !StringCointainsI(it->second.name, search)) {
        continue;
      }

      vitMapAddressBook.push_back(it);
    }

    auto comparator = sortAsc
        ? [](const AddressBookIter &a, const AddressBookIter &b) -> bool {
      return a->second.name.compare(b->second.name) < 0;
    }
    : [](const AddressBookIter &a, const AddressBookIter &b) -> bool {
        return b->second.name.compare(a->second.name) < 0;
      };
    std::sort(vitMapAddressBook.begin(), vitMapAddressBook.end(), comparator);

    int numEntries = 0;
    for (auto vit = vitMapAddressBook.begin() + offset;
         vit != vitMapAddressBook.end() && numEntries < count; ++vit) {
      auto &item = *vit;
      UniValue entry(UniValue::VOBJ);

      entry.pushKV("address", EncodeDestination(item->first));
      entry.pushKV("label", item->second.name);
      entry.pushKV("owned", UniValue(addressIsMine[item->first]));

      result.push_back(entry);
      ++numEntries;
    }
  }  // cs_wallet

  return result;
}

static UniValue AddAddress(CWallet *pwallet, const std::string &address,
                           const std::string &label, const std::string &purpose,
                           const CTxDestination &dest) {
  auto it = pwallet->mapAddressBook.find(dest);
  if (it != pwallet->mapAddressBook.end()) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Address '%s' is recorded in the address book.", address));
  }
  if (!pwallet->SetAddressBook(dest, label, purpose)) {
    throw JSONRPCError(RPC_WALLET_ERROR, "SetAddressBook failed.");
  }

  UniValue result(UniValue::VOBJ);

  result.pushKV("action", "add");
  result.pushKV("address", address);

  result.pushKV("label", label);
  result.pushKV("purpose", purpose);

  result.pushKV("result", "success");

  return result;
}

static UniValue EditAddress(CWallet *pwallet, const std::string &address,
                            const std::string &label,
                            const std::string &purpose, bool setPurpose,
                            const CTxDestination &dest) {
  auto addressBookIt = pwallet->mapAddressBook.find(dest);
  if (addressBookIt == pwallet->mapAddressBook.end()) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Address '%s' is not in the address book.", address));
  }

  if (!pwallet->SetAddressBook(
          dest, label, setPurpose ? purpose : addressBookIt->second.purpose)) {
    throw JSONRPCError(RPC_WALLET_ERROR, "SetAddressBook failed.");
  }

  UniValue result(UniValue::VOBJ);
  result.pushKV("action", "edit");
  result.pushKV("address", address);

  result.pushKV("label", addressBookIt->second.name);
  result.pushKV("purpose", addressBookIt->second.purpose);

  result.pushKV("owned", UniValue(!!IsMine(*pwallet, addressBookIt->first)));

  UniValue objDestData(UniValue::VOBJ);
  for (const auto &pair : addressBookIt->second.destdata) {
    objDestData.pushKV(pair.first, pair.second);
  }
  if (!objDestData.empty()) {
    result.pushKV("destdata", objDestData);
  }

  result.pushKV("result", "success");
  return result;
}

static UniValue DeleteAddress(CWallet *pwallet, const std::string &address,
                              const CTxDestination &dest) {
  auto addressBookIt = pwallet->mapAddressBook.find(dest);
  if (addressBookIt == pwallet->mapAddressBook.end()) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Address '%s' is not in the address book.", address));
  }

  UniValue result(UniValue::VOBJ);

  result.pushKV("action", "del");
  result.pushKV("address", address);

  result.pushKV("label", addressBookIt->second.name);
  result.pushKV("purpose", addressBookIt->second.purpose);

  if (!pwallet->DelAddressBook(dest)) {
    throw JSONRPCError(RPC_WALLET_ERROR, "DelAddressBook failed.");
  }

  return result;
}

static UniValue NewSend(CWallet *pwallet, const std::string &address,
                        const std::string &label, const std::string &purpose,
                        const CTxDestination &dest) {
  auto addressBookIt = pwallet->mapAddressBook.find(dest);
  // Only update the purpose field if address does not yet exist
  std::string newPurpose;  // Empty string means don't change purpose
  if (addressBookIt == pwallet->mapAddressBook.end()) {
    newPurpose = purpose;
  }

  if (!pwallet->SetAddressBook(dest, label, newPurpose)) {
    throw JSONRPCError(RPC_WALLET_ERROR, "SetAddressBook failed.");
  }

  UniValue result(UniValue::VOBJ);
  result.pushKV("action", "newsend");
  result.pushKV("address", address);

  result.pushKV("label", label);

  if (addressBookIt != pwallet->mapAddressBook.end()) {
    result.pushKV("purpose", addressBookIt->second.purpose);
  } else {
    result.pushKV("purpose", purpose);
  }
  return result;
}

static UniValue AddressInfo(CWallet *pwallet, const std::string &address,
                            const CTxDestination &dest) {
  auto addressBookIt = pwallet->mapAddressBook.find(dest);
  if (addressBookIt == pwallet->mapAddressBook.end()) {
    throw JSONRPCError(
        RPC_INVALID_PARAMETER,
        strprintf("Address '%s' is not in the address book.", address));
  }

  UniValue result(UniValue::VOBJ);

  result.pushKV("action", "info");
  result.pushKV("address", address);

  result.pushKV("label", addressBookIt->second.name);
  result.pushKV("purpose", addressBookIt->second.purpose);

  result.pushKV("owned", UniValue(!!IsMine(*pwallet, addressBookIt->first)));

  UniValue objDestData(UniValue::VOBJ);
  for (const auto &pair : addressBookIt->second.destdata) {
    objDestData.pushKV(pair.first, pair.second);
  }
  if (!objDestData.empty()) {
    result.pushKV("destdata", objDestData);
  }

  result.pushKV("result", "success");

  return result;
}

UniValue manageaddressbook(const JSONRPCRequest &request) {
  CWallet *pwallet = GetWalletForJSONRPCRequest(request);
  if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
    return NullUniValue;
  }

  if (request.fHelp || request.params.size() < 2 || request.params.size() > 4) {
    throw std::runtime_error(
        "manageaddressbook \"action\" \"address\" ( \"label\" \"purpose\" )\n"
        "\nManage the address book.\n"
        "\nArguments:\n"
        "1. \"action\"      (string, required) 'add/edit/del/info/newsend' The "
        "action to take.\n"
        "2. \"address\"     (string, required) The address to affect.\n"
        "3. \"label\"       (string, optional) Optional label.\n"
        "4. \"purpose\"     (string, optional) Optional purpose label.\n");
  }

  ObserveSafeMode();

  // Make sure the results are valid at least up to the most recent block
  // the user could have gotten from another RPC command prior to now
  pwallet->BlockUntilSyncedToCurrentChain();

  std::string action = request.params[0].get_str();
  std::string address = request.params[1].get_str();
  std::string label, purpose;

  if (action != "info") {
    EnsureWalletIsUnlocked(pwallet);
  }

  bool fHavePurpose = false;
  if (request.params.size() > 2) {
    label = request.params[2].get_str();
  }
  if (request.params.size() > 3) {
    purpose = request.params[3].get_str();
    fHavePurpose = true;
  }

  CTxDestination dest = DecodeDestination(address);
  if (!IsValidDestination(dest)) {
    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid UnitE address");
  }

  if (action == "add") {
    return AddAddress(pwallet, address, label, purpose, dest);
  } else if (action == "edit") {
    if (request.params.size() < 3) {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Need a parameter to change.");
    }
    return EditAddress(pwallet, address, label, purpose, fHavePurpose, dest);
  } else if (action == "del") {
    return DeleteAddress(pwallet, address, dest);
  } else if (action == "info") {
    return AddressInfo(pwallet, address, dest);
  } else if (action == "newsend") {
    return NewSend(pwallet, address, label, purpose, dest);
  } else {
    throw JSONRPCError(RPC_INVALID_PARAMETER,
                       "Unknown action, must be one of 'add/edit/del'.");
  }
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    {"wallet",             "addressbookinfo",        &addressbookinfo,        {}},
    {"wallet",             "filteraddresses",        &filteraddresses,        {"offset", "count", "sort_code", "search", "match_owned"}},
    {"wallet",             "manageaddressbook",      &manageaddressbook,      {"action", "address", "label", "purpose"}},
};
// clang-format on

void RegisterAddressbookRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
