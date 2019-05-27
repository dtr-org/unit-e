// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_NET_PROCESSING_H
#define UNITE_NET_PROCESSING_H

#include <net.h>
#include <validationinterface.h>
#include <consensus/params.h>
#include <netmessagemaker.h>
#include <sync.h>

extern CCriticalSection cs_main;

/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Default number of orphan+recently-replaced txn to keep around for block reconstruction */
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;
/** Default for BIP61 (sending reject messages) */
static constexpr bool DEFAULT_ENABLE_BIP61{true};

class PeerLogicValidation final : public CValidationInterface, public NetEventsInterface {
private:
    CConnman* const connman;
    BanMan* const m_banman;

    bool SendRejectsAndCheckIfBanned(CNode* pnode, bool enable_bip61) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
public:
    PeerLogicValidation(CConnman* connman, BanMan* banman, CScheduler &scheduler, bool enable_bip61);

    /**
     * Overridden from CValidationInterface.
     */
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex* pindexConnected, const std::vector<CTransactionRef>& vtxConflicted) override;
    /**
     * Overridden from CValidationInterface.
     */
    void UpdatedBlockTip(const CBlockIndex *pindexNew, const CBlockIndex *pindexFork, bool fInitialDownload) override;
    /**
     * Overridden from CValidationInterface.
     */
    void BlockChecked(const CBlock& block, const CValidationState& state) override;
    /**
     * Overridden from CValidationInterface.
     */
    void NewPoWValidBlock(const CBlockIndex *pindex, const std::shared_ptr<const CBlock>& pblock) override;

    /** Initialize a peer by adding it to mapNodeState and pushing a message requesting its version */
    void InitializeNode(CNode* pnode) override;
    /** Handle removal of a peer by updating various state and removing it from mapNodeState */
    void FinalizeNode(NodeId nodeid, bool& fUpdateConnectionTime) override;
    /**
    * Process protocol messages received from a given node
    *
    * @param[in]   pfrom           The node which we have received messages from.
    * @param[in]   interrupt       Interrupt condition for processing threads
    */
    bool ProcessMessages(CNode* pfrom, std::atomic<bool>& interrupt) override;
    /**
    * Send queued protocol messages to be sent to a give node.
    *
    * @param[in]   pto             The node which we are sending messages to.
    * @param[in]   node_index      The node index starting from 0 that is being called
    * @param[in]   total_nodes     The number of all nodes that will be called
    * @return                      True if there is more work to be done
    */
    bool SendMessages(CNode* pto, size_t node_index, size_t total_nodes) override;

    /** Consider evicting an outbound peer based on the amount of time they've been behind our tip */
    void ConsiderEviction(CNode *pto, int64_t time_in_seconds) EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /** Evict extra outbound peers. If we think our tip may be stale, connect to an extra outbound */
    void CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams);
    /** If we have extra outbound peers, try to disconnect the one with the oldest block announcement */
    void EvictExtraOutboundPeers(int64_t time_in_seconds) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

private:
    int64_t m_stale_tip_check_time; //!< Next time to check for stale tip

    /** Enable BIP61 (sending reject messages) */
    const bool m_enable_bip61;
};

struct CNodeStateStats {
    int nMisbehavior = 0;
    int nSyncHeight = -1;
    int nCommonHeight = -1;
    std::vector<int> vHeightInFlight;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);

/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch, const std::string& message="");

void UpdateBlockAvailability(NodeId nodeid, const uint256 &hash);

template <typename... Args>
void PushMessage(CNode &to, const std::string &command, Args&&... args) {
  const CNetMsgMaker msg_maker(to.GetSendVersion());
  g_connman->PushMessage(&to, msg_maker.Make(command, std::forward<Args>(args)...));
}

// Requires cs_main.
// Returns a bool indicating whether we requested this block.
// Also used if a block was /not/ received and timed out or started with another peer
bool MarkBlockAsReceived(const uint256& hash);

extern CCriticalSection g_cs_orphans;

struct COrphanTx {
  CTransactionRef tx;
  NodeId fromPeer;
  int64_t nTimeExpire;
  size_t list_pos;
};

extern std::map<uint256, COrphanTx> mapOrphanTransactions GUARDED_BY(g_cs_orphans);

/**
 * Sources of received blocks, saved to be able to send them reject
 * messages or ban them when processing happens afterwards. Protected by
 * cs_main.
 * Set mapBlockSource[hash].second to false if the node should not be
 * punished if the block is invalid.
 */
extern std::map<uint256, std::pair<NodeId, bool>> mapBlockSource;

#endif // UNITE_NET_PROCESSING_H
