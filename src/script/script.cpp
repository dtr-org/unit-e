// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script.h>

#include <tinyformat.h>
#include <utilstrencodings.h>

const char* GetOpName(opcodetype opcode)
{
    switch (opcode)
    {
    // push value
    case OP_0                      : return "0";
    case OP_PUSHDATA1              : return "OP_PUSHDATA1";
    case OP_PUSHDATA2              : return "OP_PUSHDATA2";
    case OP_PUSHDATA4              : return "OP_PUSHDATA4";
    case OP_1NEGATE                : return "-1";
    case OP_RESERVED               : return "OP_RESERVED";
    case OP_1                      : return "1";
    case OP_2                      : return "2";
    case OP_3                      : return "3";
    case OP_4                      : return "4";
    case OP_5                      : return "5";
    case OP_6                      : return "6";
    case OP_7                      : return "7";
    case OP_8                      : return "8";
    case OP_9                      : return "9";
    case OP_10                     : return "10";
    case OP_11                     : return "11";
    case OP_12                     : return "12";
    case OP_13                     : return "13";
    case OP_14                     : return "14";
    case OP_15                     : return "15";
    case OP_16                     : return "16";

    // control
    case OP_NOP                    : return "OP_NOP";
    case OP_VER                    : return "OP_VER";
    case OP_IF                     : return "OP_IF";
    case OP_NOTIF                  : return "OP_NOTIF";
    case OP_VERIF                  : return "OP_VERIF";
    case OP_VERNOTIF               : return "OP_VERNOTIF";
    case OP_ELSE                   : return "OP_ELSE";
    case OP_ENDIF                  : return "OP_ENDIF";
    case OP_VERIFY                 : return "OP_VERIFY";
    case OP_RETURN                 : return "OP_RETURN";

    // stack ops
    case OP_TOALTSTACK             : return "OP_TOALTSTACK";
    case OP_FROMALTSTACK           : return "OP_FROMALTSTACK";
    case OP_2DROP                  : return "OP_2DROP";
    case OP_2DUP                   : return "OP_2DUP";
    case OP_3DUP                   : return "OP_3DUP";
    case OP_2OVER                  : return "OP_2OVER";
    case OP_2ROT                   : return "OP_2ROT";
    case OP_2SWAP                  : return "OP_2SWAP";
    case OP_IFDUP                  : return "OP_IFDUP";
    case OP_DEPTH                  : return "OP_DEPTH";
    case OP_DROP                   : return "OP_DROP";
    case OP_DUP                    : return "OP_DUP";
    case OP_NIP                    : return "OP_NIP";
    case OP_OVER                   : return "OP_OVER";
    case OP_PICK                   : return "OP_PICK";
    case OP_ROLL                   : return "OP_ROLL";
    case OP_ROT                    : return "OP_ROT";
    case OP_SWAP                   : return "OP_SWAP";
    case OP_TUCK                   : return "OP_TUCK";

    // splice ops
    case OP_CAT                    : return "OP_CAT";
    case OP_SUBSTR                 : return "OP_SUBSTR";
    case OP_LEFT                   : return "OP_LEFT";
    case OP_RIGHT                  : return "OP_RIGHT";
    case OP_SIZE                   : return "OP_SIZE";

    // bit logic
    case OP_INVERT                 : return "OP_INVERT";
    case OP_AND                    : return "OP_AND";
    case OP_OR                     : return "OP_OR";
    case OP_XOR                    : return "OP_XOR";
    case OP_EQUAL                  : return "OP_EQUAL";
    case OP_EQUALVERIFY            : return "OP_EQUALVERIFY";
    case OP_RESERVED1              : return "OP_RESERVED1";
    case OP_RESERVED2              : return "OP_RESERVED2";

    // numeric
    case OP_1ADD                   : return "OP_1ADD";
    case OP_1SUB                   : return "OP_1SUB";
    case OP_2MUL                   : return "OP_2MUL";
    case OP_2DIV                   : return "OP_2DIV";
    case OP_NEGATE                 : return "OP_NEGATE";
    case OP_ABS                    : return "OP_ABS";
    case OP_NOT                    : return "OP_NOT";
    case OP_0NOTEQUAL              : return "OP_0NOTEQUAL";
    case OP_ADD                    : return "OP_ADD";
    case OP_SUB                    : return "OP_SUB";
    case OP_MUL                    : return "OP_MUL";
    case OP_DIV                    : return "OP_DIV";
    case OP_MOD                    : return "OP_MOD";
    case OP_LSHIFT                 : return "OP_LSHIFT";
    case OP_RSHIFT                 : return "OP_RSHIFT";
    case OP_BOOLAND                : return "OP_BOOLAND";
    case OP_BOOLOR                 : return "OP_BOOLOR";
    case OP_NUMEQUAL               : return "OP_NUMEQUAL";
    case OP_NUMEQUALVERIFY         : return "OP_NUMEQUALVERIFY";
    case OP_NUMNOTEQUAL            : return "OP_NUMNOTEQUAL";
    case OP_LESSTHAN               : return "OP_LESSTHAN";
    case OP_GREATERTHAN            : return "OP_GREATERTHAN";
    case OP_LESSTHANOREQUAL        : return "OP_LESSTHANOREQUAL";
    case OP_GREATERTHANOREQUAL     : return "OP_GREATERTHANOREQUAL";
    case OP_MIN                    : return "OP_MIN";
    case OP_MAX                    : return "OP_MAX";
    case OP_WITHIN                 : return "OP_WITHIN";

    // crypto
    case OP_RIPEMD160              : return "OP_RIPEMD160";
    case OP_SHA1                   : return "OP_SHA1";
    case OP_SHA256                 : return "OP_SHA256";
    case OP_HASH160                : return "OP_HASH160";
    case OP_HASH256                : return "OP_HASH256";
    case OP_CODESEPARATOR          : return "OP_CODESEPARATOR";
    case OP_CHECKSIG               : return "OP_CHECKSIG";
    case OP_CHECKSIGVERIFY         : return "OP_CHECKSIGVERIFY";
    case OP_CHECKMULTISIG          : return "OP_CHECKMULTISIG";
    case OP_CHECKMULTISIGVERIFY    : return "OP_CHECKMULTISIGVERIFY";

    // expansion
    case OP_NOP1                   : return "OP_NOP1";
    case OP_CHECKLOCKTIMEVERIFY    : return "OP_CHECKLOCKTIMEVERIFY";
    case OP_CHECKSEQUENCEVERIFY    : return "OP_CHECKSEQUENCEVERIFY";
    case OP_CHECKVOTESIG           : return "OP_CHECKVOTESIG";
    case OP_SLASHABLE              : return "OP_SLASHABLE";
    case OP_NOP6                   : return "OP_NOP6";
    case OP_NOP7                   : return "OP_NOP7";
    case OP_NOP8                   : return "OP_NOP8";
    case OP_PUSH_TX_TYPE           : return "OP_PUSH_TX_TYPE";
    case OP_NOP10                  : return "OP_NOP10";

    case OP_INVALIDOPCODE          : return "OP_INVALIDOPCODE";

    // Note:
    //  The template matching params OP_SMALLINTEGER/etc are defined in opcodetype enum
    //  as kind of implementation hack, they are *NOT* real opcodes.  If found in real
    //  Script, just let the default: case deal with them.

    default:
        return "OP_UNKNOWN";
    }
}

unsigned int CScript::GetSigOpCount(bool fAccurate) const
{
    unsigned int n = 0;
    const_iterator pc = begin();
    opcodetype lastOpcode = OP_INVALIDOPCODE;
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            break;
        if (opcode == OP_CHECKSIG || opcode == OP_CHECKSIGVERIFY)
            n++;
        else if (opcode == OP_CHECKMULTISIG || opcode == OP_CHECKMULTISIGVERIFY)
        {
            if (fAccurate && lastOpcode >= OP_1 && lastOpcode <= OP_16)
                n += DecodeOP_N(lastOpcode);
            else
                n += MAX_PUBKEYS_PER_MULTISIG;
        }
        lastOpcode = opcode;
    }
    return n;
}

unsigned int CScript::GetSigOpCount(const CScript& scriptSig) const
{
    if (!IsPayToScriptHash())
        return GetSigOpCount(true);

    // This is a pay-to-script-hash scriptPubKey;
    // get the last item that the scriptSig
    // pushes onto the stack:
    const_iterator pc = scriptSig.begin();
    std::vector<unsigned char> vData;
    while (pc < scriptSig.end())
    {
        opcodetype opcode;
        if (!scriptSig.GetOp(pc, opcode, vData))
            return 0;
        if (opcode > OP_16)
            return 0;
    }

    /// ... and return its opcount:
    CScript subscript(vData.begin(), vData.end());
    return subscript.GetSigOpCount(true);
}

bool CScript::IsPayToPublicKeyHash() const
{
    // Extra-fast test for pay-to-pubkey-hash CScripts:
    return (this->size() == 25 &&
        (*this)[0] == OP_DUP &&
        (*this)[1] == OP_HASH160 &&
        (*this)[2] == 0x14 &&
        (*this)[23] == OP_EQUALVERIFY &&
        (*this)[24] == OP_CHECKSIG);
}

CScript CScript::CreatePayVoteSlashScript(const CPubKey &pubkey)
{
    return CScript() <<
                     ToByteVector(pubkey) <<
                     OP_CHECKVOTESIG <<

                     OP_IF << OP_TRUE << OP_ELSE <<

                     ToByteVector(pubkey) <<
                     OP_SLASHABLE <<

                     OP_NOTIF <<

                     OP_DUP <<
                     OP_HASH160 <<
                     ToByteVector(pubkey.GetID()) <<
                     OP_EQUALVERIFY <<
                     OP_CHECKSIG <<

                     OP_ELSE <<
                     OP_TRUE <<

                     OP_ENDIF <<
                     OP_ENDIF;
}

CScript CScript::CreateUnspendableScript() {
    return CScript() << OP_RETURN;
}

CScript CScript::CreateP2PKHScript(const std::vector<unsigned char> &publicKeyHash) {
    return CScript() << OP_DUP << OP_HASH160
            << publicKeyHash << OP_EQUALVERIFY << OP_CHECKSIG;
}

bool CScript::MatchPayToPublicKeyHash(size_t ofs) const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() - ofs >= 25 &&
        (*this)[ofs + 0] == OP_DUP &&
        (*this)[ofs + 1] == OP_HASH160 &&
        (*this)[ofs + 2] == 0x14 &&
        (*this)[ofs + 23] == OP_EQUALVERIFY &&
        (*this)[ofs + 24] == OP_CHECKSIG);
}

bool CScript::MatchPayVoteSlashScript(size_t ofs) const
{
    // Extra-fast test for pay-vote-slash script hash CScripts:
    return (this->size() - ofs == 103 &&
        this->MatchVoteScript(0) &&

        (*this)[ofs + 35] == OP_IF &&
        (*this)[ofs + 36] == OP_TRUE &&

        (*this)[ofs + 37] == OP_ELSE &&

        this->MatchSlashScript(38) &&

        (*this)[ofs + 73] == OP_NOTIF &&

        this->MatchPayToPublicKeyHash(74) &&

        (*this)[ofs + 99] == OP_ELSE &&
        (*this)[ofs + 100] == OP_TRUE &&
        (*this)[ofs + 101] == OP_ENDIF &&
        (*this)[ofs + 102] == OP_ENDIF);
}

bool CScript::IsPayVoteSlashScript() const
{
    return (this->size() == 103 && this->MatchPayVoteSlashScript(0));
}

bool CScript::MatchVoteScript(size_t ofs) const
{
    return (this->size() - ofs >= 35 &&
        (*this)[ofs + 0] == 0x21 &&
        (*this)[ofs + 34] == OP_CHECKVOTESIG);
}

bool CScript::MatchSlashScript(size_t ofs) const
{
    return (this->size() - ofs >= 35 &&
        (*this)[ofs + 0] == 0x21 &&
        (*this)[ofs + 34] == OP_SLASHABLE);
}

bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            (*this)[0] == OP_HASH160 &&
            (*this)[1] == 0x14 &&
            (*this)[22] == OP_EQUAL);
}

bool CScript::IsPayToWitnessScriptHash() const
{
    // Extra-fast test for pay-to-witness-script-hash CScripts:
    return (this->size() == 34 &&
            (*this)[0] == OP_0 &&
            (*this)[1] == 0x20);
}

// A witness program is any valid CScript that consists of a 1-byte push opcode
// followed by a data push between 2 and 40 bytes.
bool CScript::IsWitnessProgram() const
{
    if (this->size() < 4 || this->size() > 42) {
        return false;
    }

    opcodetype opcode;
    auto pc = begin();
    if (!GetOp(pc, opcode)) {
        return false;
    }
    if (opcode != OP_0 && (opcode < OP_1 || opcode > OP_16)) {
        return false;
    }
    if (opcode == OP_0) {
        return (size_t)((*this)[1] + 2) == this->size();
    }

    do {
        if (!GetOp(pc, opcode)) {
            return false;
        }
        if (opcode == OP_0 || opcode >= OP_PUSHDATA1) {
            return false;
        }
    } while (pc < end());
    return true;
}

bool CScript::ExtractWitnessProgram(WitnessProgram &witness_program) const
{
    if (!IsWitnessProgram()) {
        return false;
    }

    opcodetype opcode;
    auto pc = begin();
    if (!GetOp(pc, opcode)) {
        return false;
    }
    witness_program.m_version = DecodeOP_N(opcode);

    std::vector<unsigned char> data;

    witness_program.m_program.clear();
    do {
        if (!GetOp(pc, opcode, data)) {
            return false;
        }
        witness_program.m_program.emplace_back(std::move(data));
    } while (pc < end());

    return true;
}

bool CScript::IsPushOnly(const_iterator pc) const
{
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            return false;
        // Note that IsPushOnly() *does* consider OP_RESERVED to be a
        // push-type opcode, however execution of OP_RESERVED fails, so
        // it's not relevant to P2SH/BIP62 as the scriptSig would fail prior to
        // the P2SH special validation code being executed.
        if (opcode > OP_16)
            return false;
    }
    return true;
}

bool CScript::IsPushOnly() const
{
    return this->IsPushOnly(begin());
}

std::string CScriptWitness::ToString() const
{
    std::string ret = "CScriptWitness(";
    for (unsigned int i = 0; i < stack.size(); i++) {
        if (i) {
            ret += ", ";
        }
        ret += HexStr(stack[i]);
    }
    return ret + ")";
}

bool CScript::HasValidOps() const
{
    CScript::const_iterator it = begin();
    while (it < end()) {
        opcodetype opcode;
        std::vector<unsigned char> item;
        if (!GetOp(it, opcode, item) || opcode > MAX_OPCODE || item.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            return false;
        }
    }
    return true;
}


//UNIT-E: this can be probably optimized for faster access
bool CScript::DecodeVote(const CScript &script, esperanza::Vote &voteOut, std::vector<unsigned char> &voteSig)
{
    CScript::const_iterator it = script.begin();
    opcodetype opcode;

    //Recover the voteSig
    if (!script.GetOp(it, opcode, voteSig)) {
      return false;
    }
    std::vector<unsigned char> validator;
    if (!script.GetOp(it, opcode, validator)) {
      return false;
    }

    if (validator.size() != CHash160::OUTPUT_SIZE) {
      return false;
    }

    uint160 validatorAddress(validator);

    std::vector<unsigned char> target;
    if (!script.GetOp(it, opcode, target)) {
      return false;
    }

    if (target.size() != CHash256::OUTPUT_SIZE) {
      return false;
    }

    uint256 targetHash(target);

    std::vector<unsigned char> sourceEpochVec;
    if (!script.GetOp(it, opcode, sourceEpochVec)) {
      return false;
    }

    uint32_t sourceEpoch = 0;
    if (!CScriptNum::deserialize(sourceEpochVec, sourceEpoch)) {
      return false;
    }

    std::vector<unsigned char> targetEpochVec;
    if (!script.GetOp(it, opcode, targetEpochVec)) {
      return false;
    }

    uint32_t targetEpoch = 0;
    if (!CScriptNum::deserialize(targetEpochVec, targetEpoch)) {
      return false;
    }

    voteOut.m_validatorAddress = validatorAddress;
    voteOut.m_targetHash = targetHash;
    voteOut.m_sourceEpoch = sourceEpoch;
    voteOut.m_targetEpoch = targetEpoch;

    return it == script.end();
}

CScript CScript::EncodeVote(const esperanza::Vote &data,
                            const std::vector<unsigned char> &voteSig)
{
    assert(!voteSig.empty());

    return CScript() << voteSig
                     << ToByteVector(data.m_validatorAddress)
                     << ToByteVector(data.m_targetHash)
                     << CScriptNum::serialize(data.m_sourceEpoch)
                     << CScriptNum::serialize(data.m_targetEpoch);
}

bool CScript::ExtractVoteFromWitness(const CScriptWitness &witness,
    esperanza::Vote &voteOut,
    std::vector<unsigned char> &voteSig)
{
    CScriptWitness wt{witness};

    //We want to skip the first element since is the signature of the transaction
    auto it = ++(wt.stack.begin());

    CScript voteScript(it->begin(), it->end());

    return DecodeVote(voteScript, voteOut, voteSig);
}

bool CScript::ExtractVoteFromVoteSignature(const CScript &scriptSig,
                                                  esperanza::Vote &voteOut,
                                                  std::vector<unsigned char> &voteSigOut)
{
    const_iterator pc = scriptSig.begin();
    std::vector<unsigned char> vData;
    opcodetype opcode;

    //Skip the first value (txSig)
    if (!scriptSig.GetOp(pc, opcode)) {
        return false;
    }

    //Unpack the vote
    if (!scriptSig.GetOp(pc, opcode, vData)) {
        return false;
    }
    CScript voteScript(vData.begin(), vData.end());
    if (!DecodeVote(voteScript, voteOut, voteSigOut)) {
      return false;
    }

    return true;
}

bool CScript::ExtractVotesFromSlashSignature(const CScript &scriptSig,
                                              esperanza::Vote &vote1,
                                              esperanza::Vote &vote2,
                                              std::vector<unsigned char> &vote1Sig,
                                              std::vector<unsigned char> &vote2Sig)
{
  const_iterator pc = scriptSig.begin();
  std::vector<unsigned char> vData;
  opcodetype opcode;

  //Skip the first value (txSig)
  if(!scriptSig.GetOp(pc, opcode)) {
      return false;
  }

  //Unpack the first vote
  if (!scriptSig.GetOp(pc, opcode, vData)) {
    return false;
  }
  CScript voteScript = CScript(vData.begin(), vData.end());
  if (!DecodeVote(voteScript, vote1, vote1Sig)) {
    return false;
  }

  //Unpack the second vote
  if (!scriptSig.GetOp(pc, opcode, vData)) {
    return false;
  }
  voteScript = CScript(vData.begin(), vData.end());
  if (!DecodeVote(voteScript, vote2, vote2Sig)) {
    return false;
  }

  return true;
}

bool CScript::ExtractAdminKeysFromWitness(const CScriptWitness &witness,
                                          std::vector<CPubKey> &outKeys)
{
    // stack is expected to look like:
    // empty
    // signature
    // ...
    // signature
    // <OP_N> <PubKey> ... <PubKey> <OP_M> <OP_CHECKMULTISIG>

    if (witness.stack.size() < 2) {
        return false;
    }

    opcodetype opcode;
    std::vector<uint8_t> buffer;

    const auto &witnessBack = witness.stack.back();
    CScript script(witnessBack.begin(), witnessBack.end());
    CScript::const_iterator it = script.begin();

    // Ignore OP_N
    if (!script.GetOp(it, opcode)) {
        return false;
    }

    while (script.GetOp(it, opcode, buffer)) {
        if (buffer.size() == CPubKey::COMPRESSED_PUBLIC_KEY_SIZE) {
            outKeys.emplace_back(CPubKey(buffer.begin(), buffer.end()));
        } else {
            // It is either OP_M or something invalid
            break;
        }
    }

    if (!script.GetOp(it, opcode) || opcode != OP_CHECKMULTISIG) {
        return false;
    }

    return it == script.end();
}

const std::vector<unsigned char> &WitnessProgram::GetV0Program() const
{
    assert(m_version == 0 && m_program.size() == 1);
    return m_program[0];
}

bool WitnessProgram::IsPayToScriptHash() const
{
    return m_version == 0 && m_program.size() == 1 && m_program[0].size() == 32;
}

bool WitnessProgram::IsPayToPubkeyHash() const
{
    return m_version == 0 && m_program.size() == 1 && m_program[0].size() == 20;
}

bool WitnessProgram::IsRemoteStaking() const
{
    return m_version == 1 && m_program.size() == 2 && m_program[0].size() == 20
        && m_program[1].size() == 32;
}
