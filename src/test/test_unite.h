// Copyright (c) 2015-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TEST_TEST_UNITE_H
#define UNITE_TEST_TEST_UNITE_H

#include <blockchain/blockchain_behavior.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <fs.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <scheduler.h>
#include <txdb.h>
#include <txmempool.h>

#include <memory>

#include <boost/thread.hpp>

extern uint256 insecure_rand_seed;
extern FastRandomContext insecure_rand_ctx;

static inline void SeedInsecureRand(bool fDeterministic = false)
{
    if (fDeterministic) {
        insecure_rand_seed = uint256();
    } else {
        insecure_rand_seed = GetRandHash();
    }
    insecure_rand_ctx = FastRandomContext(insecure_rand_seed);
}

static inline uint16_t InsecureRand16() { return static_cast<uint16_t>(insecure_rand_ctx.rand32() % 65536); }
static inline uint32_t InsecureRand32() { return insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return insecure_rand_ctx.randbool(); }

static inline void InsecureNewKey(CKey &key, bool fCompressed) {
  uint256 i = InsecureRand256();
  key.Set(i.begin(), i.end(), fCompressed);
  assert(key.IsValid()); // Failure should be very rare
}

//! Configures almost as much as the BasicTestingSetup
//! except for chain params - useful for testing stuff
//! that is actually blockchain agnostic, yet requires
//! a bit of infrastructure like logging or ECC_Start.
//! This comment was carefully crafted such that every
//! line would have the same number of characters, yo.
struct ReducedTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit ReducedTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~ReducedTestingSetup();
};

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup : public ReducedTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit BasicTestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~BasicTestingSetup();

  // todo: remove after merging https://github.com/bitcoin/bitcoin/commit/d3dae3ddf9fa95d652dfdf44bb496617513644a2
  fs::path SetDataDir(const std::string& name);
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads setup.
 */
class CConnman;
class CNode;
struct CConnmanTest {
    static void AddNode(CNode& node, CConnman *connman);
    static void ClearNodes(CConnman *connman);
    static void StartThreadMessageHandler(CConnman *connman);
};

class PeerLogicValidation;
struct TestingSetup: public BasicTestingSetup {
    fs::path pathTemp;
    boost::thread_group threadGroup;
    CConnman* connman;
    CScheduler scheduler;
    std::unique_ptr<PeerLogicValidation> peerLogic;

    explicit TestingSetup(const std::string& chainName = CBaseChainParams::MAIN);
    ~TestingSetup();
};

class CBlock;
struct CMutableTransaction;
class CScript;

class CTxMemPoolEntry;

struct TestMemPoolEntryHelper
{
    // Default values
    CAmount nFee;
    int64_t nTime;
    unsigned int nHeight;
    bool spendsCoinbase;
    unsigned int sigOpCost;
    LockPoints lp;

    TestMemPoolEntryHelper() :
        nFee(0), nTime(0), nHeight(1),
        spendsCoinbase(false), sigOpCost(4) { }

    CTxMemPoolEntry FromTx(const CMutableTransaction &tx);
    CTxMemPoolEntry FromTx(const CTransaction &tx);

    // Change the default value
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(unsigned int _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
};

CBlock getBlock13b8a();

//! utility function to set the global network parameters
void SelectNetwork(const std::string& network_name);

// define an implicit conversion here so that uint256 may be used directly in BOOST_CHECK_*
std::ostream& operator<<(std::ostream& os, const uint256& num);

// To extend boost logging with custom types, we have to extend <<(std::ostream, T).
// Extention must be defined in the same namespace as T.
// To extend << for std::vector, we have to put operator<< in namespace std.
// https://www.boost.org/doc/libs/1_69_0/libs/test/doc/html/boost_test/test_output/test_tools_support_for_logging/testing_tool_output_disable.html
namespace std {
template <typename T>
::std::ostream &operator<<(::std::ostream &os, const ::std::vector<T> &v) {
    os << ::util::to_string(v);
    return os;
}
template <typename T>
::std::ostream &operator<<(::std::ostream &os, const ::std::set<T> &v) {
  os << ::util::to_string(v);
  return os;
}
}

#endif
