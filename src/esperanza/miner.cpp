// Copyright (c) 2018 The unit-e core developers
// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <address/address.h>
#include <chainparams.h>
#include <validation.h>
#include <esperanza/validation.h>
#include <esperanza/kernel.h>
#include <esperanza/miner.h>
#include <esperanza/stakethread.h>
#include <rpc/blockchain.h>

#include <util.h>
#include <utilmoneystr.h>

#include <atomic>
#include <memory>

namespace esperanza {

double GetPoSKernelPS() {
  LOCK(cs_main);

  CBlockIndex *pindex = chainActive.Tip();
  CBlockIndex *pindexPrevStake = nullptr;

  int nBestHeight = pindex->nHeight;

  int nPoSInterval = 72; // blocks sampled
  double dStakeKernelsTriedAvg = 0;
  int nStakesHandled = 0, nStakesTime = 0;

  while (pindex && nStakesHandled < nPoSInterval) {
    if (pindexPrevStake) {
      dStakeKernelsTriedAvg += GetDifficulty(pindexPrevStake) * 4294967296.0;
      nStakesTime += pindexPrevStake->nTime - pindex->nTime;
      nStakesHandled++;
    }
    pindexPrevStake = pindex;
    pindex = pindex->pprev;
  }

  double result = 0;

  if (nStakesTime) {
    result = dStakeKernelsTriedAvg / nStakesTime;
  }

  result *= ::Params().EsperanzaParams().GetStakeTimestampMask(nBestHeight) + 1;

  return result;
}

bool CheckStake(CBlock *pblock) {
  uint256 proofHash, hashTarget;
  uint256 hashBlock = pblock->GetHash();

  if (!pblock->IsProofOfStake()) {
    return error("%s: %s is not a proof-of-stake block.", __func__, hashBlock.GetHex());
  }
  if (!esperanza::CheckStakeUnique(*pblock, false)) { // Check in SignBlock also
    return error("%s: %s CheckStakeUnique failed.", __func__, hashBlock.GetHex());
  }

  BlockMap::const_iterator mi = mapBlockIndex.find(pblock->hashPrevBlock);
  if (mi == mapBlockIndex.end()) {
    return error("%s: %s prev block not found: %s.", __func__, hashBlock.GetHex(), pblock->hashPrevBlock.GetHex());
  }
  if (!chainActive.Contains(mi->second)) {
    return error("%s: %s prev block in active chain: %s.",
                 __func__,
                 hashBlock.GetHex(),
                 pblock->hashPrevBlock.GetHex());
  }
  // verify hash target and signature of coinstake tx
  if (!esperanza::CheckProofOfStake(mi->second,
                                    *pblock->vtx[0],
                                    pblock->nTime,
                                    pblock->nBits,
                                    proofHash,
                                    hashTarget)) {
    return error("%s: proof-of-stake checking failed.", __func__);
  }

  // debug print
  LogPrintf("CheckStake(): New proof-of-stake block found  \n  hash: %s \nproofhash: %s  \ntarget: %s\n",
            hashBlock.GetHex(),
            proofHash.GetHex(),
            hashTarget.GetHex());
  if (LogAcceptCategory(BCLog::POS)) {
    LogPrintf("block %s\n", pblock->ToString());
    LogPrintf("out %s\n", FormatMoney(pblock->vtx[0]->GetValueOut()));
  }

  {
    LOCK(cs_main);
    if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash()) // hashbestchain
      return error("%s: Generated block is stale.", __func__);
  }

  std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
  if (!ProcessNewBlock(::Params(), shared_pblock, true, nullptr)) {
    return error("%s: Block not accepted.", __func__);
  }
  return true;
}

bool ImportOutputs(CBlockTemplate *pblocktemplate, int nHeight) {
  LogPrint(BCLog::POS, "%s, nHeight %d\n", __func__, nHeight);

  CBlock *pblock = &pblocktemplate->block;
  if (pblock->vtx.size() < 1) {
    return error("%s: Malformed block.", __func__);
  }

  fs::path fPath = GetDataDir() / "genesisOutputs.txt";

  if (!fs::exists(fPath)) {
    return error("%s: File not found 'genesisOutputs.txt'.", __func__);
  }

  const int nMaxOutputsPerTxn = 80;
  FILE *fp;
  errno = 0;
  if (!(fp = fopen(fPath.string().c_str(), "rb")))
    return error("%s - Can't open file, strerror: %s.", __func__, strerror(errno));

  CMutableTransaction txn;
  txn.SetVersion(0); // todo: define version number fields
  //txn.SetType(TXN_COINBASE);
  txn.nLockTime = 0;
  txn.vin.push_back(CTxIn()); // null prevout

  // scriptsig len must be > 2
  const char *s = "import";
  txn.vin[0].scriptSig =
      CScript() << std::vector<unsigned char>((const unsigned char *) s, (const unsigned char *) s + strlen(s));

  int nOutput = 0, nAdded = 0;
  char cLine[512];
  char *pAddress, *pAmount;

  while (fgets(cLine, 512, fp)) {
    cLine[511] = '\0'; // safety
    size_t len = strlen(cLine);
    while (isspace(cLine[len - 1]) && len > 0) {
      cLine[len - 1] = '\0', len--;
    }

    if (!(pAddress = strtok(cLine, ",")) || !(pAmount = strtok(nullptr, ","))) {
      continue;
    }
    nOutput++;
    if (nOutput <= nMaxOutputsPerTxn * (nHeight - 1)) {
      continue;
    }
    errno = 0;
    uint64_t amount = strtoull(pAmount, nullptr, 10);
    if (errno || !MoneyRange(amount)) {
      LogPrintf("Warning: %s - Skipping invalid amount: %s, %s\n", __func__, pAmount, strerror(errno));
      continue;
    }

    std::string addrStr(pAddress);
    address::Address addr(addrStr);

    CKeyID id;
    if (!addr.IsValid() || !addr.GetKeyID(id)) {
      LogPrintf("Warning: %s - Skipping invalid address: %s\n", __func__, pAddress);
      continue;
    }

    CScript script = CScript() << OP_DUP << OP_HASH160 << ToByteVector(id) << OP_EQUALVERIFY << OP_CHECKSIG;
    CTxOut txout(amount, script);
    txn.vout.push_back(txout);

    nAdded++;
    if (nAdded >= nMaxOutputsPerTxn) {
      break;
    }
  }

  fclose(fp);

  uint256 hash = txn.GetHash();
  if (!::Params().CheckImportCoinbase(nHeight, hash)) {
    return error("%s - Incorrect outputs hash.", __func__);
  }

  pblock->vtx.insert(pblock->vtx.begin() + 1, MakeTransactionRef(txn));

  return true;
};

} // namespace esperanza
