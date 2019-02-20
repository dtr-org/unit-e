// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <chrono>
#include <stats_logs/stats_collector.h>
#include <util.h>


namespace stats_logs {

// Used in GetInstance methods
std::atomic_bool StatsCollector::created_global_instance(false);

//! WARNING: Don't call this class method before calling its parametrized version!
StatsCollector& StatsCollector::GetInstance() {
    if (!StatsCollector::created_global_instance.load()) {
        // Trick to avoid creating a not usable StatsCollector global instance.
        static StatsCollector dummy("", 1000);
        return dummy;
    }
    // The parameters don't have effect since we got back a static variable
    return StatsCollector::GetInstance("", 1000);
}

//! Be aware that there will be a unique instance, even if we call the function
//! with different parameters. Better call it just once.
StatsCollector& StatsCollector::GetInstance(
    std::string output_filename,
    uint32_t sampling_interval
) {
    static StatsCollector instance(std::move(output_filename), sampling_interval);
    LogPrintf("Accessing the StatsCollector global instance (%s)\n", instance.output_filename);

    StatsCollector::created_global_instance.store(true);
    return instance;
}

StatsCollector::~StatsCollector() {
    StopSampling();
}

void StatsCollector::StartSampling() {
    if (StatsCollectorStates::PENDING != state) {
        return;  // We start sampling just once
    }
    state = StatsCollectorStates::STARTING;

    output_file.open(output_filename);
    assert(output_file.good());
    LogPrintf("Opened StatsCollector output file (%s)\n", output_filename);

    sampling_thread = std::thread([this]() { this->SampleForever(); });
}

void StatsCollector::SampleForever() {
    state = StatsCollectorStates::SAMPLING;
    LogPrintf("Started StatsCollector sampling thread\n");

    while (StatsCollectorStates::SAMPLING == state) {
        Sample();
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval));
    }
}

void StatsCollector::Sample() {
    assert(StatsCollectorStates::SAMPLING == state);

    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        duration
    ).count();

    output_file
        << timestamp                << ','
        << height                   << ','
        << mempool_num_transactions << ','
        << mempool_used_memory      << ','
        << peers_num_inbound        << ','
        << peers_num_outbound       << ','
        << tip_stats_active         << ','
        << tip_stats_valid_fork     << ','
        << tip_stats_valid_header   << ','
        << tip_stats_headers_only   << ','
        << tip_stats_invalid
        << std::endl;
}

void StatsCollector::StopSampling() {
    if (
        StatsCollectorStates::CLOSED == state ||
        StatsCollectorStates::CLOSING == state
    ) {
        return;  // There's no need to continue
    }

    state = StatsCollectorStates::CLOSING;

    if (sampling_thread.joinable()) {
        sampling_thread.join();
    }

    if (output_file.is_open()) {
        output_file.flush();
        output_file.close();
    }

    state = StatsCollectorStates::CLOSED;
}

void StatsCollector::SetHeight(uint32_t value) {
    height = value;
}

void StatsCollector::SetMempoolNumTransactions(uint32_t value) {
    mempool_num_transactions = value;
}

void StatsCollector::SetMempoolUsedMemory(uint64_t value) {
    mempool_used_memory = value;
}

void StatsCollector::SetTipStatsActive(uint16_t value) {
    tip_stats_active = value;
}

void StatsCollector::SetTipStatsValidFork(uint16_t value) {
    tip_stats_valid_fork = value;
}

void StatsCollector::SetTipStatsValidHeader(uint16_t value) {
    tip_stats_valid_header = value;
}

void StatsCollector::SetTipStatsHeadersOnly(uint16_t value) {
    tip_stats_headers_only = value;
}

void StatsCollector::SetTipStatsInvalid(uint16_t value) {
    tip_stats_invalid = value;
}

void StatsCollector::SetPeersStats(uint16_t num_inbound, uint16_t num_outbound) {
    peers_num_inbound = num_inbound;
    peers_num_outbound = num_outbound;
}

}
