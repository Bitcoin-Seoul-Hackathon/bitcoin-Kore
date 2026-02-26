// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_MINER_H
#define BITCOIN_NODE_MINER_H

#include <interfaces/types.h>
#include <node/types.h>
#include <policy/policy.h>
#include <primitives/block.h>
#include <txmempool.h>
#include <util/feefrac.h>

#include <cstdint>
#include <memory>
#include <optional>

#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

class ArgsManager;
class CBlockIndex;
class CChainParams;
class CScript;
class Chainstate;
class ChainstateManager;

namespace Consensus { struct Params; };

using interfaces::BlockRef;

namespace node {
class KernelNotifications;

static const bool DEFAULT_PRINT_MODIFIED_FEE = false;

struct CBlockTemplate
{
    CBlock block;
    // 코인베이스 트랜잭션을 제외한 트랜잭션별 수수료 (CBlock::vtx와 달리 코인베이스 미포함).
    // Fees per transaction, not including coinbase transaction (unlike CBlock::vtx).
    std::vector<CAmount> vTxFees;
    // 코인베이스 트랜잭션을 제외한 트랜잭션별 SigOps (CBlock::vtx와 달리 코인베이스 미포함).
    // Sigops per transaction, not including coinbase transaction (unlike CBlock::vtx).
    std::vector<int64_t> vTxSigOpsCost;
    /* 블록 템플릿에 포함되도록 패키지가 선택된 순서대로 정렬된
     * 패키지 수수료율 벡터. */
    /* A vector of package fee rates, ordered by the sequence in which
     * packages are selected for inclusion in the block template.*/
    std::vector<FeePerVSize> m_package_feerates;
    /*
     * 마이너 코드에서 설정하는 코인베이스 트랜잭션 필드들을 모두 담은 템플릿.
     */
    /*
     * Template containing all coinbase transaction fields that are set by our
     * miner code.
     */
    CoinbaseTx m_coinbase_tx;
};

/** 유효한 작업증명 없이 새 블록을 생성 */
/** Generate a new block, without valid proof-of-work */
class BlockAssembler
{
private:
    // 생성된 블록 템플릿
    // The constructed block template
    std::unique_ptr<CBlockTemplate> pblocktemplate;

    // 블록의 현재 상태 정보
    // Information on the current status of the block
    uint64_t nBlockWeight;
    uint64_t nBlockTx;
    uint64_t nBlockSigOpsCost;
    CAmount nFees;

    // 블록을 위한 체인 컨텍스트
    // Chain context for the block
    int nHeight;
    int64_t m_lock_time_cutoff;

    const CChainParams& chainparams;
    const CTxMemPool* const m_mempool;
    Chainstate& m_chainstate;

public:
    struct Options : BlockCreateOptions {
	// 블록 크기(가중치) 관련 설정 파라미터
        // Configuration parameters for the block size
        size_t nBlockMaxWeight{DEFAULT_BLOCK_MAX_WEIGHT};
        CFeeRate blockMinFeeRate{DEFAULT_BLOCK_MIN_TX_FEE};
	// CreateNewBlock() 마지막에 TestBlockValidity()를 호출할지 여부.
        // Whether to call TestBlockValidity() at the end of CreateNewBlock().
        bool test_block_validity{true};
        bool print_modified_fee{DEFAULT_PRINT_MODIFIED_FEE};
    };

    explicit BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool, const Options& options);

    /** 새 블록 템플릿을 구성 */
    /** Construct a new block template */
    std::unique_ptr<CBlockTemplate> CreateNewBlock();

    /** 마지막으로 조립된 블록의 트랜잭션 개수(코인베이스 트랜잭션 제외) */
    /** The number of transactions in the last assembled block (excluding coinbase transaction) */
    inline static std::optional<int64_t> m_last_block_num_txs{};
    /** 마지막으로 조립된 블록의 가중치(블록 헤더, tx 개수, 코인베이스 tx를 위한 예약 가중치 포함) */
    /** The weight of the last assembled block (including reserved weight for block header, txs count and coinbase tx) */
    inline static std::optional<int64_t> m_last_block_weight{};

private:
    const Options m_options;

    // 유틸리티 함수들
    /** 블록의 상태를 초기화하고 새 블록 조립을 준비 */
    // utility functions
    /** Clear the block's state and prepare for assembling a new block */
    void resetBlock();
    /** 블록에 트랜잭션 추가 */
    /** Add a tx to the block */
    void AddToBlock(const CTxMemPoolEntry& entry);

    // 블록에 트랜잭션을 추가하는 방식들.
    /** 청크 수수료율(chunk feerate)에 기반해 트랜잭션을 추가
      *
      * @pre BlockAssembler::m_mempool 은 nullptr 이 아니어야 함
    */    
    // Methods for how to add transactions to a block.
    /** Add transactions based on chunk feerate
      *
      * @pre BlockAssembler::m_mempool must not be nullptr
    */
    void addChunks() EXCLUSIVE_LOCKS_REQUIRED(m_mempool->cs);

    // addChunks()용 헬퍼 함수들
    /** 새 청크가 블록에 "들어갈 수 있는지" 테스트 */    
    // helper functions for addChunks()
    /** Test if a new chunk would "fit" in the block */
    bool TestChunkBlockLimits(FeePerWeight chunk_feerate, int64_t chunk_sigops_cost) const;
    /** 청크 내 각 트랜잭션에 대해 locktime 검사를 수행:
      * 이 검사는 항상 성공해야 하며,
      * 버그가 있을 경우를 대비한 추가 검사로만 존재함 */
    /** Perform locktime checks on each transaction in a chunk:
      * This check should always succeed, and is here
      * only as an extra check in case of a bug */
    bool TestChunkTransactions(const std::vector<CTxMemPoolEntryRef>& txs) const;
};

/**
 * 다음 블록에서 마이너가 사용해야 하는 최소 시간을 가져옵니다. 이는 항상
 * BIP94 timewarp 규칙을 반영하므로, 합의(consensus) 한계를 그대로 반영하지 않을 수 있습니다.
 */
/**
 * Get the minimum time a miner should use in the next block. This always
 * accounts for the BIP94 timewarp rule, so does not necessarily reflect the
 * consensus limit.
 */
int64_t GetMinimumTime(const CBlockIndex* pindexPrev, int64_t difficulty_adjustment_interval);

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev);

/** 블록 tx들이 변경된 이후, CreateNewBlock에서 생성된 오래된 GenerateCoinbaseCommitment를 업데이트 */
/** Update an old GenerateCoinbaseCommitment from CreateNewBlock after the block txs have changed */
void RegenerateCommitments(CBlock& block, ChainstateManager& chainman);

/** ArgsManager의 -blockmintxfee 및 -blockmaxweight 옵션을 BlockAssembler 옵션에 적용합니다. */
/** Apply -blockmintxfee and -blockmaxweight options from ArgsManager to BlockAssembler options. */
void ApplyArgsManOptions(const ArgsManager& gArgs, BlockAssembler::Options& options);

/* 블록의 머클 루트를 계산하고, 코인베이스 트랜잭션과 머클 루트를 블록에 삽입하거나 교체합니다 */
/* Compute the block's merkle root, insert or replace the coinbase transaction and the merkle root into the block */
void AddMerkleRootAndCoinbase(CBlock& block, CTransactionRef coinbase, uint32_t version, uint32_t timestamp, uint32_t nonce);

/* 블로킹 호출을 중단합니다. */
/* Interrupt a blocking call. */
void InterruptWait(KernelNotifications& kernel_notifications, bool& interrupt_wait);
/**
 * 수수료가 특정 임계치 이상으로 오르거나 새 tip이 생기면 새 블록 템플릿을 반환합니다.
 * 타임아웃에 도달하면 nullopt를 반환합니다.
 */
/**
 * Return a new block template when fees rise to a certain threshold or after a
 * new tip; return nullopt if timeout is reached.
 */
std::unique_ptr<CBlockTemplate> WaitAndCreateNewBlock(ChainstateManager& chainman,
                                                      KernelNotifications& kernel_notifications,
                                                      CTxMemPool* mempool,
                                                      const std::unique_ptr<CBlockTemplate>& block_template,
                                                      const BlockWaitOptions& options,
                                                      const BlockAssembler::Options& assemble_options,
                                                      bool& interrupt_wait);

/* cs_main을 잠그고 활성 체인이 존재하면 블록 해시와 높이를 반환합니다. 없으면 nullopt를 반환합니다. */
/* Locks cs_main and returns the block hash and block height of the active chain if it exists; otherwise, returns nullopt.*/
std::optional<BlockRef> GetTip(ChainstateManager& chainman);

/* 연결된 tip이 바뀔 때까지(타임아웃까지) 대기합니다. 노드 초기화 중에는 tip이 연결될 때까지( `timeout`과 무관하게) 대기합니다.
 * 노드가 종료 중이거나 interrupt()가 호출된 경우 nullopt를, 그렇지 않으면 현재 tip을 반환합니다.
 */
/* Waits for the connected tip to change until timeout has elapsed. During node initialization, this will wait until the tip is connected (regardless of `timeout`).
 * Returns the current tip, or nullopt if the node is shutting down or interrupt()
 * is called.
 */
std::optional<BlockRef> WaitTipChanged(ChainstateManager& chainman, KernelNotifications& kernel_notifications, const uint256& current_tip, MillisecondsDouble& timeout, bool& interrupt);

/**
 * 현재 체인 tip을 최선의 헤더(best known header)가 확장하고 있고, 동시에
 * 3초마다 최소 1개의 블록이 tip에 추가되는 동안 대기합니다. tip이
 * 충분히 뒤처져 있다면, 다음 tip 업데이트를 위해 최대 20초까지 허용합니다.
 *
 * 계속 기다리는 것은 안전하지 않습니다. 악의적인 마이너가 헤더만 먼저 알리고
 * 블록 공개를 지연시켜 이 소프트웨어를 사용하는 다른 모든 마이너를
 * 멈춰 서게(stall) 만들 수 있기 때문입니다. 동시에, 기본 대기 시간을 짧게 유지하되,
 * 랜덤한 블록의 다운로드(또는 처리)가 느린 경우 쿨다운을 너무 일찍 끝내지 않도록
 * 균형을 맞출 필요가 있습니다.
 *
 * 이 쿨다운은 createNewBlock()에만 적용되며, 이는 일반적으로 연결된 클라이언트마다
 * 한 번 호출됩니다. 이후 템플릿은 waitNext()가 제공합니다.
 *
 * @param last_tip 쿨다운 윈도우 시작 시점의 tip.
 * @param interrupt_mining 쿨다운을 중단하려면 true로 설정.
 *
 * @returns 중단되었으면 false.
 */
/**
 * Wait while the best known header extends the current chain tip AND at least
 * one block is being added to the tip every 3 seconds. If the tip is
 * sufficiently far behind, allow up to 20 seconds for the next tip update.
 *
 * It’s not safe to keep waiting, because a malicious miner could announce a
 * header and delay revealing the block, causing all other miners using this
 * software to stall. At the same time, we need to balance between the default
 * waiting time being brief, but not ending the cooldown prematurely when a
 * random block is slow to download (or process).
 *
 * The cooldown only applies to createNewBlock(), which is typically called
 * once per connected client. Subsequent templates are provided by waitNext().
 *
 * @param last_tip tip at the start of the cooldown window.
 * @param interrupt_mining set to true to interrupt the cooldown.
 *
 * @returns false if interrupted.
 */
bool CooldownIfHeadersAhead(ChainstateManager& chainman, KernelNotifications& kernel_notifications, const BlockRef& last_tip, bool& interrupt_mining);
} // namespace node

#endif // BITCOIN_NODE_MINER_H
