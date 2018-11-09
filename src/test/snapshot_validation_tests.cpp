// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <snapshot/snapshot_validation.h>

#include <snapshot/messages.h>
#include <test/test_unite.h>
#include <validation.h>
#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(snapshot_validation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(validate_candidate_block_tx) {
  SetDataDir("snapshot_state");
  fs::remove_all(GetDataDir() / snapshot::SNAPSHOT_FOLDER);

  {
    // all non-coinbase tx are not checked
    CMutableTransaction mtx;
    mtx.vin.emplace_back(CTxIn());
    mtx.vin.emplace_back(CTxIn());

    CTransaction tx(mtx);
    CBlockIndex idx;

    CCoinsViewCache view(pcoinsdbview.get());
    BOOST_CHECK(snapshot::ValidateCandidateBlockTx(tx, &idx, view));
  }

  {
    // missing snapshot hash
    CMutableTransaction mtx;
    CBlockIndex idx;
    idx.nHeight = 100;
    CTxIn in;
    in.scriptSig << idx.nHeight << OP_0;
    mtx.vin.emplace_back(in);
    CTransaction tx(mtx);

    CCoinsViewCache view(pcoinsdbview.get());
    BOOST_CHECK(!snapshot::ValidateCandidateBlockTx(tx, &idx, view));
  }

  {
    // snapshot hash is incorrect
    CBlockIndex block;
    block.nHeight = 100;
    CBlockIndex prevBlock;
    prevBlock.bnStakeModifier = uint256S("aa");
    block.pprev = &prevBlock;

    snapshot::SnapshotHash hash1;
    hash1.AddUTXO(snapshot::UTXO());
    BOOST_CHECK(pcoinsdbview->SetSnapshotHash(hash1));

    CMutableTransaction mtx;
    CTxIn in;

    uint256 h = snapshot::SnapshotHash().GetHash(prevBlock.bnStakeModifier);
    std::vector<uint8_t> hash(h.begin(), h.end());
    in.scriptSig << block.nHeight << hash << OP_0;
    mtx.vin.emplace_back(in);
    CTransaction tx(mtx);

    CCoinsViewCache view(pcoinsdbview.get());
    BOOST_CHECK(!snapshot::ValidateCandidateBlockTx(tx, &block, view));
  }

  {
    // snapshot hash is correct
    CBlockIndex block;
    block.nHeight = 100;
    CBlockIndex prevBlock;
    prevBlock.bnStakeModifier = uint256S("aa");
    block.pprev = &prevBlock;

    snapshot::SnapshotHash snapHash;
    snapHash.AddUTXO(snapshot::UTXO());
    BOOST_CHECK(pcoinsdbview->SetSnapshotHash(snapHash));
    uint256 hash = snapHash.GetHash(prevBlock.bnStakeModifier);

    CMutableTransaction mtx;
    CTxIn in;
    std::vector<uint8_t> data(hash.begin(), hash.end());
    in.scriptSig << block.nHeight << data << OP_0;
    mtx.vin.emplace_back(in);
    CTransaction tx(mtx);

    CCoinsViewCache view(pcoinsdbview.get());
    BOOST_CHECK(snapshot::ValidateCandidateBlockTx(tx, &block, view));
  }
}

BOOST_AUTO_TEST_SUITE_END()
