// Copyright (c) 2015-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef UNITE_TEST_TEST_UNITE_H
#define UNITE_TEST_TEST_UNITE_H

#include <blockchain/blockchain_behavior.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <fs.h>
#include <injector_config.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <scheduler.h>
#include <txdb.h>
#include <txmempool.h>

#include <memory>
#include <type_traits>

#include <boost/thread.hpp>

// Enable BOOST_CHECK_EQUAL for enum class types
template <typename T>
std::ostream& operator<<(typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream, const T& e)
{
    return stream << static_cast<typename std::underlying_type<T>::type>(e);
}

/**
 * This global and the helpers that use it are not thread-safe.
 *
 * If thread-safety is needed, the global could be made thread_local (given
 * that thread_local is supported on all architectures we support) or a
 * per-thread instance could be used in the multi-threaded test.
 */
extern FastRandomContext g_insecure_rand_ctx;

/**
 * Flag to make GetRand in random.h return the same number
 */
extern bool g_mock_deterministic_tests;

static inline void SeedInsecureRand(bool deterministic = false)
{
    g_insecure_rand_ctx = FastRandomContext(deterministic);
}

static inline uint16_t InsecureRand16() { return static_cast<uint16_t>(g_insecure_rand_ctx.rand32() % 65536); }
static inline uint32_t InsecureRand32() { return g_insecure_rand_ctx.rand32(); }
static inline uint256 InsecureRand256() { return g_insecure_rand_ctx.rand256(); }
static inline uint64_t InsecureRandBits(int bits) { return g_insecure_rand_ctx.randbits(bits); }
static inline uint64_t InsecureRandRange(uint64_t range) { return g_insecure_rand_ctx.randrange(range); }
static inline bool InsecureRandBool() { return g_insecure_rand_ctx.randbool(); }

// UNIT-E TODO [0.18.0]: Remove if the test will be fine
// static constexpr CAmount EEES{1000000};

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

    explicit ReducedTestingSetup(const std::string& chainName = CBaseChainParams::TESTNET);
    ~ReducedTestingSetup();
};

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup : public ReducedTestingSetup {
    ECCVerifyHandle globalVerifyHandle;

    explicit BasicTestingSetup(
        const std::string& chainName = CBaseChainParams::TESTNET,
        UnitEInjectorConfiguration config = UnitEInjectorConfiguration());
    ~BasicTestingSetup();

    fs::path SetDataDir(const std::string& name);

private:
    const fs::path m_path_root;
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads setup.
 */
class CConnman;
class CNode;
struct CConnmanTest : public CConnman {
    using CConnman::CConnman;
    static void AddNode(CNode& node);
    static void ClearNodes();
    static void StartThreadMessageHandler();
};

class PeerLogicValidation;
struct TestingSetup : public BasicTestingSetup {
    boost::thread_group threadGroup;
    CScheduler scheduler;

    explicit TestingSetup(
        const std::string& chainName = CBaseChainParams::TESTNET,
        UnitEInjectorConfiguration config = UnitEInjectorConfiguration());
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

    CTxMemPoolEntry FromTx(const CMutableTransaction& tx);
    CTxMemPoolEntry FromTx(const CTransactionRef& tx);

    // Change the default value
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(unsigned int _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
};

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
template <typename Tk, typename Tv>
::std::ostream &operator<<(::std::ostream &os, const ::std::map<Tk, Tv> &v) {
  os << ::util::to_string(v);
  return os;
}
template <typename T, size_t N>
::std::ostream &operator<<(::std::ostream &os, const ::std::array<T, N> &v) {
  os << ::util::to_string(v);
  return os;
}
}

#endif
