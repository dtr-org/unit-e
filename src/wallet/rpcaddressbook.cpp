#include <rpc/safemode.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <univalue.h>
#include <wallet/rpcwallet.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/walletutil.h>

static bool icompare_pred(unsigned char a, unsigned char b) {
  return std::tolower(a) == std::tolower(b);
}

static bool stringCointainsI(const std::string &sString,
                             const std::string &sFind) {
  // UNITE: TODO: unicode
  return std::search(sString.begin(), sString.end(), sFind.begin(), sFind.end(),
                     icompare_pred) != sString.end();
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
        "1. offset:      (numeric, optional) number of addresses to skip\n"
        "2. count:       (numerci, optional) number of addresses to be "
        "displayed\n"
        "3. sort_code:   (string, optional) 0 sort by label ascending,\n"
        "                1 sort by label descending, default 0\n"
        "4. search:      (string, optional) a query to search labels\n"
        "5. match_owned: (string, optional) 0 off, 1 owned, 2 non-owned,\n"
        "                default 0\n");
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
    std::string sortCode = request.params[2].get_str();
    if (sortCode != "0" && sortCode != "1") {
      throw JSONRPCError(RPC_INVALID_PARAMETER, "Unknown sort_code.");
    }
    sortAsc = sortCode == "0";
  }

  std::string search;
  if (request.params.size() > 3) {
    search = request.params[3].get_str();
  }

  bool onlyOwned = false;
  bool onlyNotOwned = false;
  if (request.params.size() > 4) {
    // 0 off/all, 1 owned, 2 non-owned
    std::string s = request.params[4].get_str();
    onlyOwned = s == "1";
    onlyNotOwned = s == "2";
  }

  UniValue result(UniValue::VARR);
  {
    LOCK(pwallet->cs_wallet);

    if (offset >= static_cast<int>(pwallet->mapAddressBook.size())) {
      throw JSONRPCError(
          RPC_INVALID_PARAMETER,
          strprintf("offset is beyond last address (%d).", offset));
    }
    std::vector<std::map<CTxDestination, CAddressBookData>::iterator>
        vitMapAddressBook;
    vitMapAddressBook.reserve(pwallet->mapAddressBook.size());

    std::map<CTxDestination, bool> addressIsMine;
    std::map<CTxDestination, CAddressBookData>::iterator it;
    for (it = pwallet->mapAddressBook.begin();
         it != pwallet->mapAddressBook.end(); ++it) {
      addressIsMine[it->first] = !!IsMine(*pwallet, it->first);

      if (onlyOwned && !addressIsMine[it->first]) {
        continue;
      } else if (onlyNotOwned && addressIsMine[it->first]) {
        continue;
      }

      if (!search.empty() && !stringCointainsI(it->second.name, search)) {
        continue;
      }

      vitMapAddressBook.push_back(it);
    }

    auto comparator =
        [sortAsc](const std::map<CTxDestination, CAddressBookData>::iterator &a,
                  const std::map<CTxDestination, CAddressBookData>::iterator &b)
        -> bool {
      return sortAsc ? a->second.name.compare(b->second.name) < 0
                     : b->second.name.compare(a->second.name) < 0;
    };
    std::sort(vitMapAddressBook.begin(), vitMapAddressBook.end(), comparator);

    std::map<uint32_t, std::string> mapKeyIndexCache;
    std::vector<std::map<CTxDestination, CAddressBookData>::iterator>::iterator
        vit;
    int numEntries = 0;
    for (vit = vitMapAddressBook.begin() + offset;
         vit != vitMapAddressBook.end() && numEntries < count; ++vit) {
      auto &item = *vit;
      UniValue entry(UniValue::VOBJ);

      entry.pushKV("address", EncodeDestination(item->first));
      entry.pushKV("label", item->second.name);
      entry.pushKV("owned", addressIsMine[item->first] ? "true" : "false");

      result.push_back(entry);
      ++numEntries;
    }
  }  // cs_wallet

  return result;
}

// clang-format off
static const CRPCCommand commands[] = {
//  category               name                      actor (function)         argNames
//  ---------------------  ------------------------  -----------------------  ------------------------------------------
    {"wallet",             "addressbookinfo",        &addressbookinfo,        {}},
    {"wallet",             "filteraddresses",        &filteraddresses,        {"offset", "count", "sort_code"}},
};
// clang-format on

void RegisterAddressbookRPCCommands(CRPCTable &t) {
  for (const auto &command : commands) {
    t.appendCommand(command.name, &command);
  }
}
