// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <chainparams.h>
#include <validation.h>
#include <streams.h>
#include <consensus/validation.h>
#include <staking/legacy_validation_interface.h>

namespace block_bench {
#include <bench/data/test_block.raw.h>
} // namespace block_bench

// These are the two major time-sinks which happen after we have fully received
// a block off the wire, but before we can relay the block on to peers using
// compact block relay.

static void DeserializeBlockTest(benchmark::State& state)
{
    CDataStream stream((const char*)block_bench::test_block,
            (const char*)block_bench::test_block + sizeof(block_bench::test_block),
            SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    while (state.KeepRunning()) {
        CBlock block;
        stream >> block;
        bool rewound = stream.Rewind(sizeof(block_bench::test_block));
        assert(rewound);
    }
}

static void DeserializeAndCheckBlockTest(benchmark::State& state)
{
    // UNIT-E TODO: This is a synthetic block, it makes sense to change it to a real block later on.
    CDataStream stream((const char*)block_bench::test_block,
            (const char*)block_bench::test_block + sizeof(block_bench::test_block),
            SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    const auto chainParams = CreateChainParams(CBaseChainParams::REGTEST);

    while (state.KeepRunning()) {
        CBlock block; // Note that CBlock caches its checked state, so we need to recreate it here
        stream >> block;
        bool rewound = stream.Rewind(sizeof(block_bench::test_block));
        assert(rewound);

        CValidationState validationState;
        auto validation = staking::LegacyValidationInterface::Old();
        bool checked = validation->CheckBlock(block, validationState, chainParams->GetConsensus());
        assert(checked);
    }
}

BENCHMARK(DeserializeBlockTest, 130);
BENCHMARK(DeserializeAndCheckBlockTest, 160);
