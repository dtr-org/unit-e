// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/ismine.h>

#include <key.h>
#include <keystore.h>
#include <script/script.h>
#include <script/sign.h>


typedef std::vector<unsigned char> valtype;

namespace {

/**
 * This is an enum that tracks the execution context of a script, similar to
 * SigVersion in script/interpreter. It is separate however because we want to
 * distinguish between top-level scriptPubKey execution and P2SH redeemScript
 * execution (a distinction that has no impact on consensus rules).
 */
enum class IsMineSigVersion
{
    TOP = 0,        //! scriptPubKey execution
    P2SH = 1,       //! P2SH redeemScript
    WITNESS_V0 = 2  //! P2WSH witness script execution
};

/**
 * This is an internal representation of isminetype + invalidity.
 * Its order is significant, as we return the max of all explored
 * possibilities.
 */
enum class IsMineResult
{
    NO = 0,          //! Not ours
    WATCH_ONLY = 1,  //! Included in watch-only balance
    SPENDABLE = 2,   //! Included in all balances
    HW_DEVICE = 6,   //! Stored in a hardware wallet
    INVALID = 7,     //! Not spendable by anyone (uncompressed pubkey in segwit, P2SH inside P2SH or witness, witness inside witness)
};

bool PermitsUncompressed(IsMineSigVersion sigversion)
{
    return sigversion == IsMineSigVersion::TOP || sigversion == IsMineSigVersion::P2SH;
}


//! Checks that we own all the keys in the same way (either all in hardware, or
//! all in the software wallet).
IsMineResult HaveKeys(const std::vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    size_t hw_key_count = 0, wallet_key_count = 0;

    for (const valtype& pubkey : pubkeys) {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if (keystore.HaveHardwareKey(keyID)) {
            hw_key_count++;
        }
        if (keystore.HaveKey(keyID)) {
            wallet_key_count++;
        }
    }

    if (wallet_key_count == pubkeys.size()) {
        return IsMineResult::SPENDABLE;
    }
    if (hw_key_count == pubkeys.size()) {
        return IsMineResult::HW_DEVICE;
    }
    return IsMineResult::NO;
}

IsMineResult IsMineInner(const CKeyStore& keystore, const CScript& scriptPubKey, IsMineSigVersion sigversion, ismineinfo *is_mine_info = nullptr)
{
    IsMineResult ret = IsMineResult::NO;

    std::vector<valtype> vSolutions;
    txnouttype whichType;
    Solver(scriptPubKey, whichType, vSolutions);

    CKeyID keyID;
    switch (whichType)
    {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
    case TX_WITNESS_UNKNOWN:
        break;
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if (!PermitsUncompressed(sigversion) && vSolutions[0].size() != 33) {
            return IsMineResult::INVALID;
        }
        if (keystore.HaveKey(keyID)) {
            ret = IsMineResult::SPENDABLE;
        }
        if (keystore.HaveHardwareKey(keyID)) {
            ret = IsMineResult::HW_DEVICE;
        }
        break;
    case TX_WITNESS_V0_KEYHASH:
    {
        if (sigversion == IsMineSigVersion::WITNESS_V0) {
            // P2WPKH inside P2WSH is invalid.
            return IsMineResult::INVALID;
        }
        if (sigversion == IsMineSigVersion::TOP && !keystore.HaveCScript(CScriptID(CScript() << OP_0 << vSolutions[0]))) {
            // We do not support bare witness outputs unless the P2SH version of it would be
            // acceptable as well. This protects against matching before segwit activates.
            // This also applies to the P2WSH case.
            break;
        }
        ret = IsMineInner(keystore, GetScriptForDestination(CKeyID(uint160(vSolutions[0]))), IsMineSigVersion::WITNESS_V0, is_mine_info);
        break;
    }
    case TX_PUBKEYHASH:
    {
        keyID = CKeyID(uint160(vSolutions[0]));
        if (!PermitsUncompressed(sigversion)) {
            CPubKey pubkey;
            if (keystore.GetPubKey(keyID, pubkey) && !pubkey.IsCompressed()) {
                return IsMineResult::INVALID;
            }
        }
        if (keystore.HaveKey(keyID)) {
            ret = IsMineResult::SPENDABLE;
        }
        if (keystore.HaveHardwareKey(keyID)) {
            ret = IsMineResult::HW_DEVICE;
        }
        break;
    }
    case TX_SCRIPTHASH:
    {
        if (sigversion != IsMineSigVersion::TOP) {
            // P2SH inside P2WSH or P2SH is invalid.
            return IsMineResult::INVALID;
        }
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            ret = IsMineInner(keystore, subscript, IsMineSigVersion::P2SH, is_mine_info);
        }
        break;
    }
    case TX_WITNESS_V0_SCRIPTHASH:
    {
        if (sigversion == IsMineSigVersion::WITNESS_V0) {
            // P2WSH inside P2WSH is invalid.
            return IsMineResult::INVALID;
        }
        if (sigversion == IsMineSigVersion::TOP && !keystore.HaveCScript(CScriptID(CScript() << OP_0 << vSolutions[0]))) {
            break;
        }
        uint160 hash;
        CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(hash.begin());
        CScriptID scriptID = CScriptID(hash);
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            ret = IsMineInner(keystore, subscript, IsMineSigVersion::WITNESS_V0, is_mine_info);
        }
        break;
    }

    case TX_MULTISIG:
    {
        // Never treat bare multisig outputs as ours (they can still be made watchonly-though)
        if (sigversion == IsMineSigVersion::TOP) {
            break;
        }

        // Only consider transactions "mine" if we own ALL the
        // keys involved. Multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        std::vector<valtype> keys(vSolutions.begin()+1, vSolutions.begin()+vSolutions.size()-1);
        if (!PermitsUncompressed(sigversion)) {
            for (size_t i = 0; i < keys.size(); i++) {
                if (keys[i].size() != 33) {
                    return IsMineResult::INVALID;
                }
            }
        }
        IsMineResult ret_all = HaveKeys(keys, keystore);
        if (ret_all != IsMineResult::NO) {
            ret = ret_all;
        }
        break;
    }

    case TX_PAYVOTESLASH:
    {
        if (vSolutions[0].size() == 33) {
            keyID = CPubKey(vSolutions[0]).GetID();
            // UNIT-E: At the moment we do not support deposit or vote transactions nested in P2SH/P2WSH
            if (sigversion != IsMineSigVersion::TOP) {
                return IsMineResult::INVALID;
            }
            if (keystore.HaveKey(keyID)) {
                return IsMineResult::SPENDABLE;
            }
            if (keystore.HaveHardwareKey(keyID)) {
                return IsMineResult::HW_DEVICE;
            }
        }
        break;
    }

    case TX_WITNESS_V1_RS_KEYHASH:
    {
        if (sigversion != IsMineSigVersion::TOP) {
            return IsMineResult::INVALID;
        }

        uint160 hash;
        CRIPEMD160().Write(&vSolutions[1][0], vSolutions[1].size()).Finalize(hash.begin());
        CKeyID spending_key_id = CKeyID(hash);

        ret = IsMineInner(keystore, GetScriptForDestination(spending_key_id), IsMineSigVersion::WITNESS_V0, is_mine_info);

        break;
    }

    case TX_WITNESS_V2_RS_SCRIPTHASH:
    {
        if (sigversion == IsMineSigVersion::WITNESS_V0 || sigversion == IsMineSigVersion::P2SH) {
            // Remote staking P2WSH inside P2WSH or P2SH is invalid.
            return IsMineResult::INVALID;
        }
        uint160 hash;
        CRIPEMD160().Write(vSolutions[1].data(), vSolutions[1].size()).Finalize(hash.begin());
        CScriptID scriptID = CScriptID(hash);
        CScript subscript;
        if (keystore.GetCScript(scriptID, subscript)) {
            ret = IsMineInner(keystore, subscript, IsMineSigVersion::WITNESS_V0, is_mine_info);
        }
        break;
    }
    }

    if (is_mine_info) {
        switch (sigversion) {
            case IsMineSigVersion::TOP:
                is_mine_info->type = whichType;
                is_mine_info->solutions = std::move(vSolutions);
                break;
            case IsMineSigVersion::P2SH:
            case IsMineSigVersion::WITNESS_V0:
                is_mine_info->p2sh_type = whichType;
                is_mine_info->p2sh_solutions = std::move(vSolutions);
                break;
        }
    }
    if (ret == IsMineResult::NO && keystore.HaveWatchOnly(scriptPubKey)) {
        ret = IsMineResult::WATCH_ONLY;
    }
    return ret;
}

} // namespace

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey, ismineinfo* is_mine_info)
{
    switch (IsMineInner(keystore, scriptPubKey, IsMineSigVersion::TOP, is_mine_info)) {
    case IsMineResult::INVALID:
    case IsMineResult::NO:
        return ISMINE_NO;
    case IsMineResult::WATCH_ONLY:
        return ISMINE_WATCH_ONLY;
    case IsMineResult::SPENDABLE:
        return ISMINE_SPENDABLE;
    case IsMineResult::HW_DEVICE:
        return ISMINE_HW_DEVICE;
    }
    assert(false);
}

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest)
{
    CScript script = GetScriptForDestination(dest);
    return IsMine(keystore, script);
}

bool IsStakeableByMe(const CKeyStore &keystore, const CScript &script_pub_key)
{
    ismineinfo is_mine_info;
    const isminetype is_mine = IsMine(keystore, script_pub_key, &is_mine_info);

    // UNIT-E TODO: Restrict to witness programs only once #212 is merged (fixes #48)
    switch (is_mine_info.type) {
        case TX_PUBKEYHASH:
        case TX_WITNESS_V0_KEYHASH:
        case TX_WITNESS_V1_RS_KEYHASH:
        case TX_WITNESS_V2_RS_SCRIPTHASH: {
            CKeyID key_id = CKeyID(uint160(is_mine_info.solutions[0]));
            CPubKey pubkey;
            if (!keystore.GetPubKey(key_id, pubkey)) {
                return false;
            }
            if (!pubkey.IsCompressed()) {
                return false;
            }
            return true;
        }
        case TX_WITNESS_V0_SCRIPTHASH:
            if (is_mine == isminetype::ISMINE_NO) {
              return false;
            }
            switch (is_mine_info.p2sh_type) {
                case TX_PUBKEY:
                    return true;
                case TX_MULTISIG: {
                    const auto num_signatures = static_cast<std::uint8_t>(is_mine_info.p2sh_solutions.front()[0]);
                    if (num_signatures != 1) {
                        // stake is signed by a single proposer only and the block carries a single
                        // signature of that proposer. 2-of-3 and similar multisig scenarios are not
                        // allowed for staking.
                        return false;
                    }
                    return true;
                }
                default:
                    return false;
            }
        default:
            return false;
    }
}

bool IsStakedRemotely(const CKeyStore &keystore, const CScript &script_pub_key)
{
    std::vector<valtype> solutions;
    txnouttype which_type;

    if (!Solver(script_pub_key, which_type, solutions)) {
        return false;
    }

    if (which_type != TX_WITNESS_V1_RS_KEYHASH && which_type != TX_WITNESS_V2_RS_SCRIPTHASH) {
        return false;
    }

    CKeyID staking_keyid = CKeyID(uint160(solutions[0]));

    // Uncompressed staking keys are not supported
    CPubKey staking_pubkey;
    if (keystore.GetPubKey(staking_keyid, staking_pubkey) &&
        !staking_pubkey.IsCompressed()) {
        return false;
    }

    // If the local node knows the staking key, the coin is not staked remotely
    if (keystore.HaveKey(staking_keyid)) {
        return false;
    }

    // The local node should be able to spend the coin
    IsMineResult mine = IsMineInner(keystore, script_pub_key, IsMineSigVersion::TOP);
    return mine == IsMineResult::SPENDABLE || mine == IsMineResult::HW_DEVICE;
}
