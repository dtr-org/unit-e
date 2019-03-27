// Copyright (c) 2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <base58.h>
#include <blockchain/blockchain_behavior.h>
#include <injector.h>
#include <keystore.h>
#include <pubkey.h>
#include <rpc/protocol.h>
#include <rpc/util.h>
#include <tinyformat.h>
#include <utilstrencodings.h>

// Converts a hex string to a public key if possible
CPubKey HexToPubKey(const std::string& hex_in)
{
    if (!IsHex(hex_in)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    CPubKey vchPubKey(ParseHex(hex_in));
    if (!vchPubKey.IsFullyValid()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid public key: " + hex_in);
    }
    return vchPubKey;
}

// Retrieves a public key for an address from the given CKeyStore
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in)
{
    CTxDestination dest = DecodeDestination(addr_in);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address: " + addr_in);
    }
    CKeyID key = GetKeyForDestination(*keystore, dest);
    if (key.IsNull()) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("%s does not refer to a key", addr_in));
    }
    CPubKey vchPubKey;
    if (!keystore->GetPubKey(key, vchPubKey)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, strprintf("no full public key for address %s", addr_in));
    }
    if (!vchPubKey.IsFullyValid()) {
       throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet contains an invalid public key");
    }
    return vchPubKey;
}

// Creates a multisig redeemscript from a given list of public keys and number required.
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys)
{
    // Gather public keys
    if (required < 1) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "a multisignature address must require at least one key to redeem");
    }
    if ((int)pubkeys.size() < required) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("not enough keys supplied (got %u keys, but need at least %d to redeem)", pubkeys.size(), required));
    }
    if (pubkeys.size() > 16) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Number of keys involved in the multisignature address creation > 16\nReduce the number");
    }

    CScript result = GetScriptForMultisig(required, pubkeys);

    if (result.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, (strprintf("redeemScript exceeds size limit: %d > %d", result.size(), MAX_SCRIPT_ELEMENT_SIZE)));
    }

    return result;
}

UniValue ToUniValue(const std::uint32_t value) {
    return UniValue(static_cast<std::int64_t>(value));
}

UniValue ToUniValue(const float value) {
    if (value > std::numeric_limits<decltype(value)>::max()) {
        return "+Inf";
    }
    if (value < std::numeric_limits<decltype(value)>::min()) {
        return "-Inf";
    }
    if (value != value) {
        return "NaN";
    }
    return value;
}

UniValue ToUniValue(const double value) {
    if (value > std::numeric_limits<decltype(value)>::max()) {
        return "+Inf";
    }
    if (value < std::numeric_limits<decltype(value)>::min()) {
        return "-Inf";
    }
    if (value != value) {
        return "NaN";
    }
    return value;
}

UniValue ToUniValue(const uint256& hash) {
    return UniValue(hash.GetHex());
}

UniValue ToUniValue(const blockchain::GenesisBlock& value) {
    UniValue result(UniValue::VOBJ);
    result.pushKV("version", ToUniValue(value.block.nVersion));
    result.pushKV("time", ToUniValue(value.block.nTime));
    {
        arith_uint256 difficulty;
        difficulty.SetCompact(value.block.nBits);
        result.pushKV("difficulty", ToUniValue(ArithToUint256(difficulty)));
    }
    const std::vector<CTxOut> &vout = value.block.vtx[0]->vout;
    UniValue p2wpkh_funds(UniValue::VARR);
    UniValue p2wsh_funds(UniValue::VARR);
    for (const CTxOut &out : vout) {
        if (out.scriptPubKey.IsPayToWitnessPublicKeyHash()) {
            UniValue funds(UniValue::VOBJ);
            funds.pushKV("amount", out.nValue);
            funds.pushKV("pub_key_hash", HexStr(out.scriptPubKey.begin() + 2, out.scriptPubKey.begin() + 22));
            p2wpkh_funds.push_back(funds);
        } else if (out.scriptPubKey.IsPayToWitnessScriptHash()) {
            UniValue funds(UniValue::VOBJ);
            funds.pushKV("amount", out.nValue);
            funds.pushKV("script_hash", HexStr(out.scriptPubKey.begin() + 2, out.scriptPubKey.begin() + 34));
            p2wsh_funds.push_back(funds);
        }
    }
    result.pushKV("p2wpkh_funds", p2wpkh_funds);
    result.pushKV("p2wsh_funds", p2wsh_funds);
    return result;
};

UniValue ToUniValue(const std::vector<unsigned char> base58_prefixes[blockchain::Base58Type::_size_constant]) {
    UniValue result(UniValue::VOBJ);
    for (const auto &type : blockchain::Base58Type::_values()) {
        std::vector<unsigned char> prefix =
            GetComponent<blockchain::Behavior>()->GetBase58Prefix(type);
        UniValue bytes(UniValue::VARR);
        for (const unsigned char byte : prefix) {
            bytes.push_back(byte);
        }
        result.pushKV(type._to_string(), bytes);
    }
    return result;
}
