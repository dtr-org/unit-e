// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_STATS_COLLECTOR_H
#define UNITE_STATS_COLLECTOR_H


#include <atomic>
#include <cassert>
#include <fstream>
#include <string>
#include <thread>


namespace stats_logs {


enum StatsCollectorStates { PENDING, STARTING, SAMPLING, CLOSING, CLOSED };


class StatsCollector final {
   private:
       // Global state
       static std::atomic_bool created_global_instance;

       // Stats Collector settings
       std::string output_filename;
       uint32_t sampling_interval;

       // State Machine
       StatsCollectorStates state;

       // Collected metrics
       uint32_t height;
       uint32_t mempool_num_transactions;
       uint64_t mempool_used_memory;
       uint16_t tip_stats_active;
       uint16_t tip_stats_valid_fork;
       uint16_t tip_stats_valid_header;
       uint16_t tip_stats_headers_only;
       uint16_t tip_stats_invalid;
       uint16_t peers_num_inbound;
       uint16_t peers_num_outbound;

       // Other resources
       std::ofstream output_file;
       std::thread sampling_thread;

       // Internal methods
       void SampleForever();
       void Sample();
   public:
       // Singleton-like accessor
       static StatsCollector& GetInstance();
       static StatsCollector& GetInstance(
           std::string output_filename,
           uint32_t sampling_interval
       );

       // Boilerplate:
       StatsCollector(
           std::string output_filename,
           uint32_t sampling_interval
       ):
           output_filename(output_filename),
           sampling_interval(sampling_interval),
           state(StatsCollectorStates::PENDING),
           height(0),
           mempool_num_transactions(0),
           mempool_used_memory(0),
           tip_stats_active(0),
           tip_stats_valid_fork(0),
           tip_stats_valid_header(0),
           tip_stats_headers_only(0),
           tip_stats_invalid(0),
           peers_num_inbound(0),
           peers_num_outbound(0) {};

       ~StatsCollector();

       // Lifecycle:
       //! \brief Starts a thread that periodically writes samples to a CSV file.
       void StartSampling();
       //! \brief Stops the sampling thread and closes used resources.
       void StopSampling();

       // Data collection:
       void SetHeight(uint32_t value);
       void SetMempoolNumTransactions(uint32_t value);
       void SetMempoolUsedMemory(uint64_t value);
       void SetTipStatsActive(uint16_t value);
       void SetTipStatsValidFork(uint16_t value);
       void SetTipStatsValidHeader(uint16_t value);
       void SetTipStatsHeadersOnly(uint16_t value);
       void SetTipStatsInvalid(uint16_t value);
       void SetPeersStats(uint16_t num_inbound, uint16_t num_outbound);
};


} // namespace stats_logs


#endif //UNIT_E_STATS_COLLECTOR_H
