// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <addrman.h>
#include <test/test_unite.h>
#include <string>
#include <boost/test/unit_test.hpp>
#include <hash.h>
#include <serialize.h>
#include <streams.h>
#include <net.h>
#include <netbase.h>
#include <chainparams.h>
#include <util.h>

#include <memory>

class CAddrManSerializationMock : public CAddrMan
{
public:
    virtual void Serialize(CDataStream& s) const = 0;

    //! Ensure that bucket placement is always the same for testing purposes.
    void MakeDeterministic()
    {
        nKey.SetNull();
        insecure_rand = FastRandomContext(true);
    }
};

class CAddrManUncorrupted : public CAddrManSerializationMock
{
public:
    void Serialize(CDataStream& s) const override
    {
        CAddrMan::Serialize(s);
    }
};

class CAddrManCorrupted : public CAddrManSerializationMock
{
public:
    void Serialize(CDataStream& s) const override
    {
        // Produces corrupt output that claims addrman has 20 addrs when it only has one addr.
        unsigned char nVersion = 1;
        s << nVersion;
        s << ((unsigned char)32);
        s << nKey;
        s << 10; // nNew
        s << 10; // nTried

        int nUBuckets = ADDRMAN_NEW_BUCKET_COUNT ^ (1 << 30);
        s << nUBuckets;

        CService serv;
        Lookup("252.1.1.1", serv, 7777, false);
        CAddress addr = CAddress(serv, NODE_NONE);
        CNetAddr resolved;
        LookupHost("252.2.2.2", resolved, false);
        CAddrInfo info = CAddrInfo(addr, resolved);
        s << info;
    }
};

CDataStream AddrmanToStream(CAddrManSerializationMock& _addrman)
{
    CDataStream ssPeersIn(SER_DISK, CLIENT_VERSION);
    ssPeersIn << FLATDATA(Params().MessageStart());
    ssPeersIn << _addrman;
    std::string str = ssPeersIn.str();
    std::vector<unsigned char> vchData(str.begin(), str.end());
    return CDataStream(vchData, SER_DISK, CLIENT_VERSION);
}

BOOST_FIXTURE_TEST_SUITE(net_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(cnode_listen_port)
{
    // test default
    unsigned short port = GetListenPort();
    BOOST_CHECK(port == Params().GetDefaultPort());
    // test set port
    unsigned short altPort = 12345;
    gArgs.SoftSetArg("-port", std::to_string(altPort));
    port = GetListenPort();
    BOOST_CHECK(port == altPort);
}

BOOST_AUTO_TEST_CASE(caddrdb_read)
{
    CAddrManUncorrupted addrmanUncorrupted;
    addrmanUncorrupted.MakeDeterministic();

    CService addr1, addr2, addr3;
    Lookup("250.7.1.1", addr1, 7182, false);
    Lookup("250.7.2.2", addr2, 9999, false);
    Lookup("250.7.3.3", addr3, 9999, false);

    // Add three addresses to new table.
    CService source;
    Lookup("252.5.1.1", source, 7182, false);
    addrmanUncorrupted.Add(CAddress(addr1, NODE_NONE), source);
    addrmanUncorrupted.Add(CAddress(addr2, NODE_NONE), source);
    addrmanUncorrupted.Add(CAddress(addr3, NODE_NONE), source);

    // Test that the de-serialization does not throw an exception.
    CDataStream ssPeers1 = AddrmanToStream(addrmanUncorrupted);
    bool exceptionThrown = false;
    CAddrMan addrman1;

    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> FLATDATA(pchMsgTmp);
        ssPeers1 >> addrman1;
    } catch (const std::exception& e) {
        exceptionThrown = true;
    }

    BOOST_CHECK(addrman1.size() == 3);
    BOOST_CHECK(exceptionThrown == false);

    // Test that CAddrDB::Read creates an addrman with the correct number of addrs.
    CDataStream ssPeers2 = AddrmanToStream(addrmanUncorrupted);

    CAddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    adb.Read(addrman2, ssPeers2);
    BOOST_CHECK(addrman2.size() == 3);
}


BOOST_AUTO_TEST_CASE(caddrdb_read_corrupted)
{
    CAddrManCorrupted addrmanCorrupted;
    addrmanCorrupted.MakeDeterministic();

    // Test that the de-serialization of corrupted addrman throws an exception.
    CDataStream ssPeers1 = AddrmanToStream(addrmanCorrupted);
    bool exceptionThrown = false;
    CAddrMan addrman1;
    BOOST_CHECK(addrman1.size() == 0);
    try {
        unsigned char pchMsgTmp[4];
        ssPeers1 >> FLATDATA(pchMsgTmp);
        ssPeers1 >> addrman1;
    } catch (const std::exception& e) {
        exceptionThrown = true;
    }
    // Even through de-serialization failed addrman is not left in a clean state.
    BOOST_CHECK(addrman1.size() == 1);
    BOOST_CHECK(exceptionThrown);

    // Test that CAddrDB::Read leaves addrman in a clean state if de-serialization fails.
    CDataStream ssPeers2 = AddrmanToStream(addrmanCorrupted);

    CAddrMan addrman2;
    CAddrDB adb;
    BOOST_CHECK(addrman2.size() == 0);
    adb.Read(addrman2, ssPeers2);
    BOOST_CHECK(addrman2.size() == 0);
}

BOOST_AUTO_TEST_CASE(cnode_simple_test)
{
    SOCKET hSocket = INVALID_SOCKET;
    NodeId id = 0;
    int height = 0;

    in_addr ipv4Addr;
    ipv4Addr.s_addr = 0xa0b0c001;

    CAddress addr = CAddress(CService(ipv4Addr, 7777), NODE_NETWORK);
    std::string pszDest = "";
    bool fInboundIn = false;

    // Test that fFeeler is false by default.
    std::unique_ptr<CNode> pnode1(new CNode(id++, NODE_NETWORK, height, hSocket, addr, 0, 0, CAddress(), pszDest, fInboundIn));
    BOOST_CHECK(pnode1->fInbound == false);
    BOOST_CHECK(pnode1->fFeeler == false);

    fInboundIn = true;
    std::unique_ptr<CNode> pnode2(new CNode(id++, NODE_NETWORK, height, hSocket, addr, 1, 1, CAddress(), pszDest, fInboundIn));
    BOOST_CHECK(pnode2->fInbound == true);
    BOOST_CHECK(pnode2->fFeeler == false);
}

class MockNetEvents : public NetEventsInterface {
public:
    int expect_total_nodes = 0;

    bool ProcessMessages(CNode* pnode, std::atomic<bool>& interrupt) override {
        if (pnode->nRecvBytes == 0) {
            ++pnode->nRecvBytes;
        } else {
            interrupt = true;
        }
        return true;
    }

    bool SendMessages(CNode* pnode, int node_index, int total_nodes, std::atomic<bool>& interrupt) override {
        BOOST_CHECK_EQUAL(pnode->nVersion, node_index);
        BOOST_CHECK_EQUAL(total_nodes, expect_total_nodes);

        ++pnode->nSendBytes;
        return true;
    }

    void InitializeNode(CNode* pnode) override {
    }

    void FinalizeNode(NodeId id, bool& update_connection_time) override {
    }
};

std::unique_ptr<CNode> MockNode() {
    uint32_t ip = 0xa0b0c001;
    in_addr s{ip};
    CService service(CNetAddr(s), 7182);
    CAddress addr(service, NODE_NONE);

    auto node = MakeUnique<CNode>(0, ServiceFlags(NODE_NETWORK | NODE_WITNESS), 0,
                                  INVALID_SOCKET, addr, 0, 0, CAddress(),
                                  "", /*fInboundIn=*/
                                  false);
    node->fSuccessfullyConnected = true;
    return node;
}

BOOST_AUTO_TEST_CASE(thread_message_handler) {
    MockNetEvents net_proc;
    net_proc.expect_total_nodes = 3;

    CConnman::Options options;
    options.m_msgproc = &net_proc;

    CConnman connman(0, 0);
    connman.Init(options);

    std::unique_ptr<CNode> node1 = MockNode();
    std::unique_ptr<CNode> node2 = MockNode();
    std::unique_ptr<CNode> node3 = MockNode();
    std::unique_ptr<CNode> node4 = MockNode();
    std::unique_ptr<CNode> node5 = MockNode();
    node1->nVersion = 0;
    node3->nVersion = 1;
    node5->nVersion = 2;

    node2->fDisconnect = true;
    node4->fDisconnect = true;
    std::vector<CNode *> nodes{
        node1.get(),
        node2.get(),
        node3.get(),
        node4.get(),
        node5.get(),
    };

    CConnmanTest::AddNode(*nodes[0], &connman);
    CConnmanTest::AddNode(*nodes[1], &connman);
    CConnmanTest::AddNode(*nodes[2], &connman);
    CConnmanTest::AddNode(*nodes[3], &connman);
    CConnmanTest::AddNode(*nodes[4], &connman);

    connman.WakeMessageHandler(); // ensure that function doesn't wait
    CConnmanTest::StartThreadMessageHandler(&connman);

    BOOST_CHECK_EQUAL(nodes[0]->nSendBytes, 1);
    BOOST_CHECK_EQUAL(nodes[1]->nSendBytes, 0); // disconnected
    BOOST_CHECK_EQUAL(nodes[2]->nSendBytes, 1);
    BOOST_CHECK_EQUAL(nodes[3]->nSendBytes, 0); // disconnected
    BOOST_CHECK_EQUAL(nodes[4]->nSendBytes, 1);

    BOOST_CHECK_EQUAL(nodes[0]->nRecvBytes, 1);
    BOOST_CHECK_EQUAL(nodes[1]->nRecvBytes, 0); // disconnected
    BOOST_CHECK_EQUAL(nodes[2]->nRecvBytes, 1);
    BOOST_CHECK_EQUAL(nodes[3]->nRecvBytes, 0); // disconnected
    BOOST_CHECK_EQUAL(nodes[4]->nRecvBytes, 1);

    CConnmanTest::ClearNodes(&connman);
}

BOOST_AUTO_TEST_SUITE_END()
