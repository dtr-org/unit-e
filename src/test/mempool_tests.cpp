// Copyright (c) 2011-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/policy.h>
#include <txmempool.h>
#include <util.h>

#include <test/test_unite.h>

#include <boost/test/unit_test.hpp>
#include <list>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(mempool_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(MempoolRemoveTest)
{
    // Test CTxMemPool::remove functionality

    TestMemPoolEntryHelper entry;
    // Parent transaction with three children,
    // and three grand-children:
    CMutableTransaction txParent;
    txParent.vin.resize(1);
    txParent.vin[0].scriptSig = CScript() << OP_11;
    txParent.vout.resize(3);
    for (int i = 0; i < 3; i++)
    {
        txParent.vout[i].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txParent.vout[i].nValue = 33000LL;
    }
    CMutableTransaction txChild[3];
    for (int i = 0; i < 3; i++)
    {
        txChild[i].vin.resize(1);
        txChild[i].vin[0].scriptSig = CScript() << OP_11;
        txChild[i].vin[0].prevout.hash = txParent.GetHash();
        txChild[i].vin[0].prevout.n = i;
        txChild[i].vout.resize(1);
        txChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txChild[i].vout[0].nValue = 11000LL;
    }
    CMutableTransaction txGrandChild[3];
    for (int i = 0; i < 3; i++)
    {
        txGrandChild[i].vin.resize(1);
        txGrandChild[i].vin[0].scriptSig = CScript() << OP_11;
        txGrandChild[i].vin[0].prevout.hash = txChild[i].GetHash();
        txGrandChild[i].vin[0].prevout.n = 0;
        txGrandChild[i].vout.resize(1);
        txGrandChild[i].vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
        txGrandChild[i].vout[0].nValue = 11000LL;
    }


    CTxMemPool testPool;

    // Nothing in pool, remove should do nothing:
    unsigned int poolSize = testPool.size();
    testPool.removeRecursive(txParent);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);

    // Just the parent:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    poolSize = testPool.size();
    testPool.removeRecursive(txParent);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 1);
    
    // Parent, children, grandchildren:
    testPool.addUnchecked(txParent.GetHash(), entry.FromTx(txParent));
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Remove Child[0], GrandChild[0] should be removed:
    poolSize = testPool.size();
    testPool.removeRecursive(txChild[0]);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 2);
    // ... make sure grandchild and child are gone:
    poolSize = testPool.size();
    testPool.removeRecursive(txGrandChild[0]);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);
    poolSize = testPool.size();
    testPool.removeRecursive(txChild[0]);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize);
    // Remove parent, all children/grandchildren should go:
    poolSize = testPool.size();
    testPool.removeRecursive(txParent);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 5);
    BOOST_CHECK_EQUAL(testPool.size(), 0);

    // Add children and grandchildren, but NOT the parent (simulate the parent being in a block)
    for (int i = 0; i < 3; i++)
    {
        testPool.addUnchecked(txChild[i].GetHash(), entry.FromTx(txChild[i]));
        testPool.addUnchecked(txGrandChild[i].GetHash(), entry.FromTx(txGrandChild[i]));
    }
    // Now remove the parent, as might happen if a block-re-org occurs but the parent cannot be
    // put into the mempool (maybe because it is non-standard):
    poolSize = testPool.size();
    testPool.removeRecursive(txParent);
    BOOST_CHECK_EQUAL(testPool.size(), poolSize - 6);
    BOOST_CHECK_EQUAL(testPool.size(), 0);
}

template<typename name>
void CheckSort(CTxMemPool &pool, std::vector<std::string> &sortedOrder)
{
    BOOST_CHECK_EQUAL(pool.size(), sortedOrder.size());
    typename CTxMemPool::indexed_transaction_set::index<name>::type::iterator it = pool.mapTx.get<name>().begin();
    int count=0;
    for (; it != pool.mapTx.get<name>().end(); ++it, ++count) {
        BOOST_CHECK_EQUAL(it->GetTx().GetHash().ToString(), sortedOrder[count]);
    }
}

BOOST_AUTO_TEST_CASE(MempoolIndexingTest)
{
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;

    /* 3rd highest fee */
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * UNIT;
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

    /* highest fee */
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * UNIT;
    pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).FromTx(tx2));

    /* lowest fee */
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * UNIT;
    pool.addUnchecked(tx3.GetHash(), entry.Fee(0LL).FromTx(tx3));

    /* 2nd highest fee */
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * UNIT;
    pool.addUnchecked(tx4.GetHash(), entry.Fee(15000LL).FromTx(tx4));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * UNIT;
    entry.nTime = 1;
    pool.addUnchecked(tx5.GetHash(), entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5);

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
    sortedOrder[0] = tx3.GetHash().ToString(); // 0
    sortedOrder[1] = tx5.GetHash().ToString(); // 10000
    sortedOrder[2] = tx1.GetHash().ToString(); // 10000
    sortedOrder[3] = tx4.GetHash().ToString(); // 15000
    sortedOrder[4] = tx2.GetHash().ToString(); // 20000
    LOCK(pool.cs);
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee but with high fee child */
    /* tx6 -> tx7 -> tx8, tx9 -> tx10 */
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * UNIT;
    pool.addUnchecked(tx6.GetHash(), entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), 6);
    // Check that at this point, tx6 is sorted low
    sortedOrder.insert(sortedOrder.begin(), tx6.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    CTxMemPool::setEntries setAncestors;
    setAncestors.insert(pool.mapTx.find(tx6.GetHash()));
    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * UNIT;
    tx7.vout[1].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[1].nValue = 1 * UNIT;

    CTxMemPool::setEntries setAncestorsCalculated;
    std::string dummy;
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(2000000LL).FromTx(tx7), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx7.GetHash(), entry.FromTx(tx7), setAncestors);
    BOOST_CHECK_EQUAL(pool.size(), 7);

    // Now tx6 should be sorted higher (high fee child): tx7, tx6, tx2, ...
    sortedOrder.erase(sortedOrder.begin());
    sortedOrder.push_back(tx6.GetHash().ToString());
    sortedOrder.push_back(tx7.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction tx8 = CMutableTransaction();
    tx8.vin.resize(1);
    tx8.vin[0].prevout = COutPoint(tx7.GetHash(), 0);
    tx8.vin[0].scriptSig = CScript() << OP_11;
    tx8.vout.resize(1);
    tx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx8.vout[0].nValue = 10 * UNIT;
    setAncestors.insert(pool.mapTx.find(tx7.GetHash()));
    pool.addUnchecked(tx8.GetHash(), entry.Fee(0LL).Time(2).FromTx(tx8), setAncestors);

    // Now tx8 should be sorted low, but tx6/tx both high
    sortedOrder.insert(sortedOrder.begin(), tx8.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    /* low fee child of tx7 */
    CMutableTransaction tx9 = CMutableTransaction();
    tx9.vin.resize(1);
    tx9.vin[0].prevout = COutPoint(tx7.GetHash(), 1);
    tx9.vin[0].scriptSig = CScript() << OP_11;
    tx9.vout.resize(1);
    tx9.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx9.vout[0].nValue = 1 * UNIT;
    pool.addUnchecked(tx9.GetHash(), entry.Fee(0LL).Time(3).FromTx(tx9), setAncestors);

    // tx9 should be sorted low
    BOOST_CHECK_EQUAL(pool.size(), 9);
    sortedOrder.insert(sortedOrder.begin(), tx9.GetHash().ToString());
    CheckSort<descendant_score>(pool, sortedOrder);

    std::vector<std::string> snapshotOrder = sortedOrder;

    setAncestors.insert(pool.mapTx.find(tx8.GetHash()));
    setAncestors.insert(pool.mapTx.find(tx9.GetHash()));
    /* tx10 depends on tx8 and tx9 and has a high fee*/
    CMutableTransaction tx10 = CMutableTransaction();
    tx10.vin.resize(2);
    tx10.vin[0].prevout = COutPoint(tx8.GetHash(), 0);
    tx10.vin[0].scriptSig = CScript() << OP_11;
    tx10.vin[1].prevout = COutPoint(tx9.GetHash(), 0);
    tx10.vin[1].scriptSig = CScript() << OP_11;
    tx10.vout.resize(1);
    tx10.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx10.vout[0].nValue = 10 * UNIT;

    setAncestorsCalculated.clear();
    BOOST_CHECK_EQUAL(pool.CalculateMemPoolAncestors(entry.Fee(200000LL).Time(4).FromTx(tx10), setAncestorsCalculated, 100, 1000000, 1000, 1000000, dummy), true);
    BOOST_CHECK(setAncestorsCalculated == setAncestors);

    pool.addUnchecked(tx10.GetHash(), entry.FromTx(tx10), setAncestors);

    /**
     *  tx8 and tx9 should both now be sorted higher
     *  Final order after tx10 is added:
     *
     *  tx3 = 0 (1)
     *  tx5 = 10000 (1)
     *  tx1 = 10000 (1)
     *  tx4 = 15000 (1)
     *  tx2 = 20000 (1)
     *  tx9 = 200k (2 txs)
     *  tx8 = 200k (2 txs)
     *  tx10 = 200k (1 tx)
     *  tx6 = 2.2M (5 txs)
     *  tx7 = 2.2M (4 txs)
     */
    sortedOrder.erase(sortedOrder.begin(), sortedOrder.begin()+2); // take out tx9, tx8 from the beginning
    sortedOrder.insert(sortedOrder.begin()+5, tx9.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+6, tx8.GetHash().ToString());
    sortedOrder.insert(sortedOrder.begin()+7, tx10.GetHash().ToString()); // tx10 is just before tx6
    CheckSort<descendant_score>(pool, sortedOrder);

    // there should be 10 transactions in the mempool
    BOOST_CHECK_EQUAL(pool.size(), 10);

    // Now try removing tx10 and verify the sort order returns to normal
    pool.removeRecursive(pool.mapTx.find(tx10.GetHash())->GetTx());
    CheckSort<descendant_score>(pool, snapshotOrder);

    pool.removeRecursive(pool.mapTx.find(tx9.GetHash())->GetTx());
    pool.removeRecursive(pool.mapTx.find(tx8.GetHash())->GetTx());
}

BOOST_AUTO_TEST_CASE(MempoolAncestorIndexingTest)
{
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;

    /* 3rd highest fee */
    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * UNIT;
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

    /* highest fee */
    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx2.vout[0].nValue = 2 * UNIT;
    pool.addUnchecked(tx2.GetHash(), entry.Fee(20000LL).FromTx(tx2));
    uint64_t tx2Size = GetVirtualTransactionSize(tx2);

    /* lowest fee */
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx3.vout[0].nValue = 5 * UNIT;
    pool.addUnchecked(tx3.GetHash(), entry.Fee(0LL).FromTx(tx3));

    /* 2nd highest fee */
    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vout.resize(1);
    tx4.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx4.vout[0].nValue = 6 * UNIT;
    pool.addUnchecked(tx4.GetHash(), entry.Fee(15000LL).FromTx(tx4));

    /* equal fee rate to tx1, but newer */
    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vout.resize(1);
    tx5.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx5.vout[0].nValue = 11 * UNIT;
    pool.addUnchecked(tx5.GetHash(), entry.Fee(10000LL).FromTx(tx5));
    BOOST_CHECK_EQUAL(pool.size(), 5);

    std::vector<std::string> sortedOrder;
    sortedOrder.resize(5);
    sortedOrder[0] = tx2.GetHash().ToString(); // 20000
    sortedOrder[1] = tx4.GetHash().ToString(); // 15000
    // tx1 and tx5 are both 10000
    // Ties are broken by hash, not timestamp, so determine which
    // hash comes first.
    if (tx1.GetHash() < tx5.GetHash()) {
        sortedOrder[2] = tx1.GetHash().ToString();
        sortedOrder[3] = tx5.GetHash().ToString();
    } else {
        sortedOrder[2] = tx5.GetHash().ToString();
        sortedOrder[3] = tx1.GetHash().ToString();
    }
    sortedOrder[4] = tx3.GetHash().ToString(); // 0

    LOCK(pool.cs);
    CheckSort<ancestor_score>(pool, sortedOrder);

    /* low fee parent with high fee child */
    /* tx6 (0) -> tx7 (high) */
    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vout.resize(1);
    tx6.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx6.vout[0].nValue = 20 * UNIT;
    uint64_t tx6Size = GetVirtualTransactionSize(tx6);

    pool.addUnchecked(tx6.GetHash(), entry.Fee(0LL).FromTx(tx6));
    BOOST_CHECK_EQUAL(pool.size(), 6);
    // Ties are broken by hash
    if (tx3.GetHash() < tx6.GetHash())
        sortedOrder.push_back(tx6.GetHash().ToString());
    else
        sortedOrder.insert(sortedOrder.end()-1,tx6.GetHash().ToString());

    CheckSort<ancestor_score>(pool, sortedOrder);

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(1);
    tx7.vin[0].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_11;
    tx7.vout.resize(1);
    tx7.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * UNIT;
    uint64_t tx7Size = GetVirtualTransactionSize(tx7);

    /* set the fee to just below tx2's feerate when including ancestor */
    CAmount fee = (20000/tx2Size)*(tx7Size + tx6Size) - 1;

    pool.addUnchecked(tx7.GetHash(), entry.Fee(fee).FromTx(tx7));
    BOOST_CHECK_EQUAL(pool.size(), 7);
    sortedOrder.insert(sortedOrder.begin()+1, tx7.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);

    /* after tx6 is mined, tx7 should move up in the sort */
    std::vector<CTransactionRef> vtx;
    vtx.push_back(MakeTransactionRef(tx6));
    pool.removeForBlock(vtx, 1);

    sortedOrder.erase(sortedOrder.begin()+1);
    // Ties are broken by hash
    if (tx3.GetHash() < tx6.GetHash())
        sortedOrder.pop_back();
    else
        sortedOrder.erase(sortedOrder.end()-2);
    sortedOrder.insert(sortedOrder.begin(), tx7.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);

    // High-fee parent, low-fee child
    // tx7 -> tx8
    CMutableTransaction tx8 = CMutableTransaction();
    tx8.vin.resize(1);
    tx8.vin[0].prevout  = COutPoint(tx7.GetHash(), 0);
    tx8.vin[0].scriptSig = CScript() << OP_11;
    tx8.vout.resize(1);
    tx8.vout[0].scriptPubKey = CScript() << OP_11 << OP_EQUAL;
    tx8.vout[0].nValue = 10*UNIT;

    // Check that we sort by min(feerate, ancestor_feerate):
    // set the fee so that the ancestor feerate is above tx1/5,
    // but the transaction's own feerate is lower
    pool.addUnchecked(tx8.GetHash(), entry.Fee(5000LL).FromTx(tx8));
    sortedOrder.insert(sortedOrder.end()-1, tx8.GetHash().ToString());
    CheckSort<ancestor_score>(pool, sortedOrder);
}


BOOST_AUTO_TEST_CASE(MempoolSizeLimitTest)
{
    CTxMemPool pool;
    TestMemPoolEntryHelper entry;

    CMutableTransaction tx1 = CMutableTransaction();
    tx1.vin.resize(1);
    tx1.vin[0].scriptSig = CScript() << OP_1;
    tx1.vout.resize(1);
    tx1.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
    tx1.vout[0].nValue = 10 * UNIT;
    pool.addUnchecked(tx1.GetHash(), entry.Fee(10000LL).FromTx(tx1));

    CMutableTransaction tx2 = CMutableTransaction();
    tx2.vin.resize(1);
    tx2.vin[0].scriptSig = CScript() << OP_2;
    tx2.vout.resize(1);
    tx2.vout[0].scriptPubKey = CScript() << OP_2 << OP_EQUAL;
    tx2.vout[0].nValue = 10 * UNIT;
    pool.addUnchecked(tx2.GetHash(), entry.Fee(5000LL).FromTx(tx2));

    pool.TrimToSize(pool.DynamicMemoryUsage()); // should do nothing
    BOOST_CHECK(pool.exists(tx1.GetHash()));
    BOOST_CHECK(pool.exists(tx2.GetHash()));

    pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4); // should remove the lower-feerate transaction
    BOOST_CHECK(pool.exists(tx1.GetHash()));
    BOOST_CHECK(!pool.exists(tx2.GetHash()));

    pool.addUnchecked(tx2.GetHash(), entry.FromTx(tx2));
    CMutableTransaction tx3 = CMutableTransaction();
    tx3.vin.resize(1);
    tx3.vin[0].prevout = COutPoint(tx2.GetHash(), 0);
    tx3.vin[0].scriptSig = CScript() << OP_2;
    tx3.vout.resize(1);
    tx3.vout[0].scriptPubKey = CScript() << OP_3 << OP_EQUAL;
    tx3.vout[0].nValue = 10 * UNIT;
    pool.addUnchecked(tx3.GetHash(), entry.Fee(20000LL).FromTx(tx3));

    pool.TrimToSize(pool.DynamicMemoryUsage() * 3 / 4); // tx3 should pay for tx2 (CPFP)
    BOOST_CHECK(!pool.exists(tx1.GetHash()));
    BOOST_CHECK(pool.exists(tx2.GetHash()));
    BOOST_CHECK(pool.exists(tx3.GetHash()));

    pool.TrimToSize(GetVirtualTransactionSize(tx1)); // mempool is limited to tx1's size in memory usage, so nothing fits
    BOOST_CHECK(!pool.exists(tx1.GetHash()));
    BOOST_CHECK(!pool.exists(tx2.GetHash()));
    BOOST_CHECK(!pool.exists(tx3.GetHash()));

    CFeeRate maxFeeRateRemoved(25000, GetVirtualTransactionSize(tx3) + GetVirtualTransactionSize(tx2));
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), maxFeeRateRemoved.GetFeePerK() + 1000);

    CMutableTransaction tx4 = CMutableTransaction();
    tx4.vin.resize(2);
    tx4.vin[0].prevout.SetNull();
    tx4.vin[0].scriptSig = CScript() << OP_4;
    tx4.vin[1].prevout.SetNull();
    tx4.vin[1].scriptSig = CScript() << OP_4;
    tx4.vout.resize(2);
    tx4.vout[0].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[0].nValue = 10 * UNIT;
    tx4.vout[1].scriptPubKey = CScript() << OP_4 << OP_EQUAL;
    tx4.vout[1].nValue = 10 * UNIT;

    CMutableTransaction tx5 = CMutableTransaction();
    tx5.vin.resize(2);
    tx5.vin[0].prevout = COutPoint(tx4.GetHash(), 0);
    tx5.vin[0].scriptSig = CScript() << OP_4;
    tx5.vin[1].prevout.SetNull();
    tx5.vin[1].scriptSig = CScript() << OP_5;
    tx5.vout.resize(2);
    tx5.vout[0].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[0].nValue = 10 * UNIT;
    tx5.vout[1].scriptPubKey = CScript() << OP_5 << OP_EQUAL;
    tx5.vout[1].nValue = 10 * UNIT;

    CMutableTransaction tx6 = CMutableTransaction();
    tx6.vin.resize(2);
    tx6.vin[0].prevout = COutPoint(tx4.GetHash(), 1);
    tx6.vin[0].scriptSig = CScript() << OP_4;
    tx6.vin[1].prevout.SetNull();
    tx6.vin[1].scriptSig = CScript() << OP_6;
    tx6.vout.resize(2);
    tx6.vout[0].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[0].nValue = 10 * UNIT;
    tx6.vout[1].scriptPubKey = CScript() << OP_6 << OP_EQUAL;
    tx6.vout[1].nValue = 10 * UNIT;

    CMutableTransaction tx7 = CMutableTransaction();
    tx7.vin.resize(2);
    tx7.vin[0].prevout = COutPoint(tx5.GetHash(), 0);
    tx7.vin[0].scriptSig = CScript() << OP_5;
    tx7.vin[1].prevout = COutPoint(tx6.GetHash(), 0);
    tx7.vin[1].scriptSig = CScript() << OP_6;
    tx7.vout.resize(2);
    tx7.vout[0].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[0].nValue = 10 * UNIT;
    tx7.vout[1].scriptPubKey = CScript() << OP_7 << OP_EQUAL;
    tx7.vout[1].nValue = 10 * UNIT;

    pool.addUnchecked(tx4.GetHash(), entry.Fee(7000LL).FromTx(tx4));
    pool.addUnchecked(tx5.GetHash(), entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(tx6.GetHash(), entry.Fee(1100LL).FromTx(tx6));
    pool.addUnchecked(tx7.GetHash(), entry.Fee(9000LL).FromTx(tx7));

    // we only require this remove, at max, 2 txn, because its not clear what we're really optimizing for aside from that
    pool.TrimToSize(pool.DynamicMemoryUsage() - 1);
    BOOST_CHECK(pool.exists(tx4.GetHash()));
    BOOST_CHECK(pool.exists(tx6.GetHash()));
    BOOST_CHECK(!pool.exists(tx7.GetHash()));

    if (!pool.exists(tx5.GetHash()))
        pool.addUnchecked(tx5.GetHash(), entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(tx7.GetHash(), entry.Fee(9000LL).FromTx(tx7));

    pool.TrimToSize(pool.DynamicMemoryUsage() / 2); // should maximize mempool size by only removing 5/7
    BOOST_CHECK(pool.exists(tx4.GetHash()));
    BOOST_CHECK(!pool.exists(tx5.GetHash()));
    BOOST_CHECK(pool.exists(tx6.GetHash()));
    BOOST_CHECK(!pool.exists(tx7.GetHash()));

    pool.addUnchecked(tx5.GetHash(), entry.Fee(1000LL).FromTx(tx5));
    pool.addUnchecked(tx7.GetHash(), entry.Fee(9000LL).FromTx(tx7));

    std::vector<CTransactionRef> vtx;
    SetMockTime(42);
    SetMockTime(42 + CTxMemPool::ROLLING_FEE_HALFLIFE);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), maxFeeRateRemoved.GetFeePerK() + 1000);
    // ... we should keep the same min fee until we get a block
    pool.removeForBlock(vtx, 1);
    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/2.0));
    // ... then feerate should drop 1/2 each halflife

    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2);
    BOOST_CHECK_EQUAL(pool.GetMinFee(pool.DynamicMemoryUsage() * 5 / 2).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/4.0));
    // ... with a 1/2 halflife when mempool is < 1/2 its target size

    SetMockTime(42 + 2*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(pool.DynamicMemoryUsage() * 9 / 2).GetFeePerK(), llround((maxFeeRateRemoved.GetFeePerK() + 1000)/8.0));
    // ... with a 1/4 halflife when mempool is < 1/4 its target size

    SetMockTime(42 + 7*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), 1000);
    // ... but feerate should never drop below 1000

    SetMockTime(42 + 8*CTxMemPool::ROLLING_FEE_HALFLIFE + CTxMemPool::ROLLING_FEE_HALFLIFE/2 + CTxMemPool::ROLLING_FEE_HALFLIFE/4);
    BOOST_CHECK_EQUAL(pool.GetMinFee(1).GetFeePerK(), 0);
    // ... unless it has gone all the way to 0 (after getting past 1000/2)

    SetMockTime(0);
}

BOOST_AUTO_TEST_CASE(DisconnectionTopologicalOrderTest)
{
    std::vector<CTransactionRef> vtx;
    vtx.reserve(13);

    CMutableTransaction first_mtx;
    first_mtx.vout.resize(2);
    first_mtx.vout[0].scriptPubKey = CScript();
    first_mtx.vout[0].nValue = 10 * UNIT;
    first_mtx.vout[1].scriptPubKey = CScript();
    first_mtx.vout[1].nValue = 10 * UNIT;
    auto first_tx_ref = MakeTransactionRef(std::move(first_mtx));
    vtx.push_back(first_tx_ref);


    //      TX1       TX3
    // TX0       TX2         ···
    //      TX1'      TX3'
    for (unsigned int i=0; i < 8; ++i) {
        if (i % 2 == 0) {
            CMutableTransaction mtx_a;
            CMutableTransaction mtx_b;

            mtx_a.vin.resize(1);
            mtx_a.vin[0].prevout = COutPoint(vtx.back()->GetHash(), 0);
            mtx_a.vin[0].scriptSig = CScript();
            mtx_a.vout.resize(1);
            mtx_a.vout[0].nValue = 10 * UNIT;
            mtx_a.vout[0].scriptPubKey = CScript();

            mtx_b.vin.resize(1);
            mtx_b.vin[0].prevout = COutPoint(vtx.back()->GetHash(), 1);
            mtx_b.vin[0].scriptSig = CScript();
            mtx_b.vout.resize(1);
            mtx_b.vout[0].nValue = 10 * UNIT;
            mtx_b.vout[0].scriptPubKey = CScript();

            vtx.emplace_back(MakeTransactionRef(std::move(mtx_a)));
            vtx.emplace_back(MakeTransactionRef(std::move(mtx_b)));
        } else {
            CMutableTransaction mtx;

            mtx.vin.resize(2);
            auto prev_ptx = vtx.rbegin();
            mtx.vin[0].prevout = COutPoint((*prev_ptx)->GetHash(), 0);
            mtx.vin[0].scriptSig = CScript();
            ++prev_ptx;
            mtx.vin[1].prevout = COutPoint((*prev_ptx)->GetHash(), 0);
            mtx.vin[1].scriptSig = CScript();
            mtx.vout.resize(2);
            mtx.vout[0].scriptPubKey = CScript();
            mtx.vout[0].nValue = 10 * UNIT;
            mtx.vout[1].scriptPubKey = CScript();
            mtx.vout[1].nValue = 10 * UNIT;

            vtx.emplace_back(MakeTransactionRef(std::move(mtx)));
        }
    }

    // We sort transactions in lexicographical order to remove the previous
    // topological order.
    std::sort(
        std::begin(vtx) + 1, std::end(vtx),
        [](const CTransactionRef &a, const CTransactionRef &b) -> bool {
          return a->GetHash().CompareLexicographically(b->GetHash()) < 0;
        }
    );

    DisconnectedBlockTransactions disconnectpool;
    disconnectpool.LoadFromBlockInTopologicalOrder(vtx); // System-Under-Test

    std::unordered_set<uint256, SaltedTxidHasher> processed_tx_hashes;
    processed_tx_hashes.insert(first_tx_ref->GetHash());

    for (
        auto it = disconnectpool.queuedTx.get<insertion_order>().rbegin();
        it != disconnectpool.queuedTx.get<insertion_order>().rend();
        ++it
    ) {
        CTransactionRef tx_ref = (*it);
        for (CTxIn tx_in : tx_ref->vin) {
            // We check that transactions have been ordered topologically
            BOOST_CHECK(processed_tx_hashes.count(tx_in.prevout.hash) > 0);
        }
        processed_tx_hashes.insert(tx_ref->GetHash());
    }

    disconnectpool.clear();
}

BOOST_AUTO_TEST_SUITE_END()
