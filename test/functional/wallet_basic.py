#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the wallet."""
from test_framework.test_framework import (
    UnitETestFramework,
    COINBASE_MATURITY,
    PROPOSER_REWARD,
    STAKE_SPLIT_THRESHOLD,
)
from test_framework.util import *

import math
from decimal import Decimal


def send_specific_output(node, txid, idx, to, amount):
    return node.sendtypeto(
        '', '', [{'address': to, 'amount': amount}], '', '', False,
        {'inputs': [{'tx': txid, 'n': idx}]}
    )


class WalletTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True

    def setup_network(self):
        self.add_nodes(4)
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all([self.nodes[0:3]])

    def check_fee_amount(self, curr_balance, balance_with_fee, fee_per_byte, tx_size):
        """Return curr_balance after asserting the fee was in range"""
        fee = balance_with_fee - curr_balance
        assert_fee_amount(fee, tx_size, fee_per_byte * 1000)
        return curr_balance

    def get_vsize(self, txn):
        return self.nodes[0].decoderawtransaction(txn)['vsize']

    def run_test(self):
        # Check that there's no UTXO on none of the nodes
        assert_equal(len(self.nodes[0].listunspent()), 0)
        assert_equal(len(self.nodes[1].listunspent()), 0)
        assert_equal(len(self.nodes[2].listunspent()), 0)

        self.log.info("Proposing blocks...")

        self.setup_stake_coins(*self.nodes[0:3])
        balance0 = self.nodes[0].initial_stake
        balance1 = self.nodes[1].initial_stake
        balance2 = self.nodes[2].initial_stake

        self.nodes[0].generatetoaddress(1, self.nodes[0].getnewaddress('', 'bech32'))

        walletinfo = self.nodes[0].getwalletinfo()

        immature_balance0 = PROPOSER_REWARD if COINBASE_MATURITY > 0 else 0
        balance0 += PROPOSER_REWARD - immature_balance0
        assert_equal(walletinfo['immature_balance'], immature_balance0)
        assert_equal(walletinfo['balance'], balance0)

        self.sync_all([self.nodes[0:3]])
        self.nodes[1].generate(COINBASE_MATURITY + 1)
        self.sync_all([self.nodes[0:3]])

        balance0 += PROPOSER_REWARD
        balance1 += PROPOSER_REWARD

        assert_equal(self.nodes[0].getbalance(), balance0)
        assert_equal(self.nodes[1].getbalance(), balance1)
        assert_equal(self.nodes[2].getbalance(), balance2)

        # Check the UTXO count for the nodes
        utxos = self.nodes[0].listunspent()
        assert_equal(len(utxos), math.ceil(balance0 / STAKE_SPLIT_THRESHOLD))
        assert_equal(len(self.nodes[1].listunspent()), math.ceil(balance1 / STAKE_SPLIT_THRESHOLD))
        assert_equal(len(self.nodes[2].listunspent()), 1)

        self.log.info("test gettxout")
        confirmed_txid, confirmed_index = utxos[0]["txid"], utxos[0]["vout"]
        # First, outputs that are unspent both in the chain and in the
        # mempool should appear with or without include_mempool
        txout = self.nodes[0].gettxout(txid=confirmed_txid, n=confirmed_index, include_mempool=False)
        node0_txout_value = txout['value']
        assert_greater_than(txout['value'], 0)
        txout = self.nodes[0].gettxout(txid=confirmed_txid, n=confirmed_index, include_mempool=True)
        assert_greater_than(txout['value'], 0)

        # Send 21 UTE from 0 to 2 using sendtoaddress call.
        # Locked memory should use at least 32 bytes to sign each transaction
        self.log.info("test getmemoryinfo")
        memory_before = self.nodes[0].getmemoryinfo()
        mempool_txid = send_specific_output(
            self.nodes[0], confirmed_txid, confirmed_index,
            to=self.nodes[2].getnewaddress(), amount=1
        )
        send_specific_output(self.nodes[0], utxos[1]['txid'], utxos[1]['vout'],
                             to=self.nodes[2].getnewaddress(), amount=Decimal('1.1'))
        memory_after = self.nodes[0].getmemoryinfo()
        assert memory_before['locked']['used'] + 64 <= memory_after['locked']['used']
        balance0 -= Decimal('2.1')
        balance2 += Decimal('2.1')

        self.log.info("test gettxout (second part)")
        # utxo spent in mempool should be visible if you exclude mempool
        # but invisible if you include mempool
        txout = self.nodes[0].gettxout(confirmed_txid, confirmed_index, False)
        assert_greater_than(txout['value'], 0)
        txout = self.nodes[0].gettxout(confirmed_txid, confirmed_index, True)
        assert txout is None
        # new utxo from mempool should be invisible if you exclude mempool
        # but visible if you include mempool
        txout = self.nodes[0].gettxout(mempool_txid, 0, False)
        assert txout is None
        txout1 = self.nodes[0].gettxout(mempool_txid, 0, True)
        txout2 = self.nodes[0].gettxout(mempool_txid, 1, True)
        # note the mempool tx will have randomly assigned indices
        # but 10 will go to node2 and the rest will go to node0
        tx = self.nodes[0].gettransaction(mempool_txid)
        # tx['fee'] is negative
        assert_equal(set([txout1['value'], txout2['value']]), set([1, node0_txout_value - 1 + tx['fee']]))
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(walletinfo['immature_balance'], 0)

        # Have node0 mine a block, thus it will collect its own fee.
        self.nodes[0].generate(1)
        self.sync_all([self.nodes[0:3]])

        # Exercise locking of unspent outputs
        unspent = [{"txid": x["txid"], "vout": x["vout"]} for x in self.nodes[2].listunspent()]
        for unspent_0 in unspent:
            assert_raises_rpc_error(-8, "Invalid parameter, expected locked output", self.nodes[2].lockunspent, True, [unspent_0])
            self.nodes[2].lockunspent(False, [unspent_0])
            assert_raises_rpc_error(-8, "Invalid parameter, output already locked", self.nodes[2].lockunspent, False, [unspent_0])

        assert_raises_rpc_error(-4, "Insufficient funds", self.nodes[2].sendtoaddress, self.nodes[2].getnewaddress(), 20)
        assert_equal(unspent, self.nodes[2].listlockunspent())
        self.nodes[2].lockunspent(True, unspent[0:1])
        assert_equal(len(self.nodes[2].listlockunspent()), len(unspent) - 1)
        assert_raises_rpc_error(-8, "Invalid parameter, unknown transaction",
                              self.nodes[2].lockunspent, False,
                              [{"txid": "0000000000000000000000000000000000", "vout": 0}])
        assert_raises_rpc_error(-8, "Invalid parameter, vout index out of bounds",
                              self.nodes[2].lockunspent, False,
                              [{"txid": unspent[0]["txid"], "vout": 999}])

        # Have node1 generate some blocks (so node0 can recover the fee)
        self.nodes[1].generate(COINBASE_MATURITY)
        self.sync_all([self.nodes[0:3]])
        balance0 += PROPOSER_REWARD

        # node0 should end up with 100 btc in block rewards plus fees, but
        # minus the 21 plus fees sent to node2
        assert_equal(self.nodes[0].getbalance(), balance0)
        assert_equal(self.nodes[2].getbalance(), balance2)

        # Node0 should have several unspent outputs.
        # Create transactions to send them to node2, submit them through
        # node1, and make sure both node0 and node2 pick them up properly:
        node0utxos = self.nodes[0].listunspent(1)
        assert_equal(len(node0utxos), math.ceil(balance0 / STAKE_SPLIT_THRESHOLD) + 1)

        # create both transactions
        txns_to_send = []
        node2_from1 = 0
        for utxo in node0utxos:
            inputs = []
            outputs = {}
            inputs.append({ "txid" : utxo["txid"], "vout" : utxo["vout"]})
            outputs[self.nodes[2].getnewaddress("from1")] = utxo["amount"] - Decimal('0.3')
            raw_tx = self.nodes[0].createrawtransaction(inputs, outputs)
            txns_to_send.append(self.nodes[0].signrawtransaction(raw_tx))
            node2_from1 += utxo["amount"] - Decimal('0.3')
        balance2 += node2_from1
        balance0 = 0

        # Have node 1 (miner) send the transactions
        for tx in txns_to_send:
            self.nodes[1].sendrawtransaction(tx["hex"], True)

        # Have node1 mine a block to confirm transactions:
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])

        assert_equal(self.nodes[0].getbalance(), 0)
        assert_equal(self.nodes[2].getbalance(), balance2)
        assert_equal(self.nodes[2].getbalance("from1"), node2_from1)

        # Verify that a spent output cannot be locked anymore
        spent_0 = {"txid": node0utxos[0]["txid"], "vout": node0utxos[0]["vout"]}
        assert_raises_rpc_error(-8, "Invalid parameter, expected unspent output", self.nodes[0].lockunspent, False, [spent_0])

        # Send 10 UTE normal
        address = self.nodes[0].getnewaddress("test")
        fee_per_byte = Decimal('0.001') / 1000
        self.nodes[2].settxfee(fee_per_byte * 1000)
        txid = self.nodes[2].sendtoaddress(address, 10, "", "", False)
        sync_mempools(self.nodes[0:3])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal = self.check_fee_amount(self.nodes[2].getbalance(), balance2 - 10, fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))
        assert_equal(self.nodes[0].getbalance(), Decimal('10'))

        # Send 10 UTE with subtract fee from amount
        txid = self.nodes[2].sendtoaddress(address, 10, "", "", True)
        sync_mempools(self.nodes[0:3])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal -= Decimal('10')
        assert_equal(self.nodes[2].getbalance(), node_2_bal)
        node_0_bal = self.check_fee_amount(self.nodes[0].getbalance(), Decimal('20'), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))

        # Sendmany 10 UTE
        txid = self.nodes[2].sendmany('from1', {address: 10}, 0, "", [])
        sync_mempools(self.nodes[0:3])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_0_bal += Decimal('10')
        node_2_bal = self.check_fee_amount(self.nodes[2].getbalance(), node_2_bal - Decimal('10'), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))
        assert_equal(self.nodes[0].getbalance(), node_0_bal)

        # Sendmany 10 UTE with subtract fee from amount
        txid = self.nodes[2].sendmany('from1', {address: 10}, 0, "", [address])
        sync_mempools(self.nodes[0:3])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal -= Decimal('10')
        assert_equal(self.nodes[2].getbalance(), node_2_bal)
        node_0_bal = self.check_fee_amount(self.nodes[0].getbalance(), node_0_bal + Decimal('10'), fee_per_byte, self.get_vsize(self.nodes[2].getrawtransaction(txid)))

        # Test ResendWalletTransactions:
        # - Start up, prepare and disconnect the fourth node (nodes[3])
        # - Create a couple of transactions
        # - Connect nodes[3] and ask nodes[0] to rebroadcast
        # EXPECT: nodes[3] should have those transactions in its mempool.
        self.start_node(3)
        connect_nodes_bi(self.nodes, 0, 3)
        sync_blocks(self.nodes)
        disconnect_nodes(self.nodes[0], self.nodes[3].index)

        txid1 = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), 1)
        txid2 = self.nodes[1].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        sync_mempools(self.nodes[0:2])

        connect_nodes(self.nodes[0], self.nodes[3].index)
        relayed = self.nodes[0].resendwallettransactions()
        assert_equal(set(relayed), {txid1, txid2})
        sync_mempools(self.nodes)

        assert txid1 in self.nodes[3].getrawmempool()

        # Exercise balance rpcs
        assert_equal(self.nodes[0].getwalletinfo()["unconfirmed_balance"], 1)
        assert_equal(self.nodes[0].getunconfirmedbalance(), 1)

        #check if we can list zero value tx as available coins
        #1. create rawtx
        #2. hex-changed one output to 0.0
        #3. sign and send
        #4. check if recipient (node0) can list the zero value tx
        usp = self.nodes[1].listunspent()
        inputs = [{"txid":usp[0]['txid'], "vout":usp[0]['vout']}]
        outputs = {self.nodes[1].getnewaddress(): usp[0]['amount'] - Decimal('0.002'), self.nodes[0].getnewaddress(): 11.11}

        rawTx = self.nodes[1].createrawtransaction(inputs, outputs).replace("c0833842", "00000000") #replace 11.11 with 0.0 (int32)
        decRawTx = self.nodes[1].decoderawtransaction(rawTx)
        signedRawTx = self.nodes[1].signrawtransaction(rawTx)
        decRawTx = self.nodes[1].decoderawtransaction(signedRawTx['hex'])
        zeroValueTxid= decRawTx['txid']
        self.nodes[1].sendrawtransaction(signedRawTx['hex'])

        self.sync_all()
        self.nodes[1].generate(1) #mine a block
        self.sync_all()

        unspentTxs = self.nodes[0].listunspent() #zero value tx must be in listunspents output
        found = False
        for uTx in unspentTxs:
            if uTx['txid'] == zeroValueTxid:
                found = True
                assert_equal(uTx['amount'], Decimal('0'))
        assert found

        #do some -walletbroadcast tests
        self.stop_nodes()
        self.start_node(0, ["-walletbroadcast=0"])
        self.start_node(1, ["-walletbroadcast=0"])
        self.start_node(2, ["-walletbroadcast=0"])
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.sync_all([self.nodes[0:3]])

        txIdNotBroadcasted  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 2)
        txObjNotBroadcasted = self.nodes[0].gettransaction(txIdNotBroadcasted)
        self.nodes[1].generate(1) #mine a block, tx should not be in there
        self.sync_all([self.nodes[0:3]])
        assert_equal(self.nodes[2].getbalance(), node_2_bal) #should not be changed because tx was not broadcasted

        #now broadcast from another node, mine a block, sync, and check the balance
        self.nodes[1].sendrawtransaction(txObjNotBroadcasted['hex'])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])
        node_2_bal += 2
        txObjNotBroadcasted = self.nodes[0].gettransaction(txIdNotBroadcasted)
        assert_equal(self.nodes[2].getbalance(), node_2_bal)

        #create another tx
        txIdNotBroadcasted  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), 2)

        #restart the nodes with -walletbroadcast=1
        self.stop_nodes()
        self.start_node(0)
        self.start_node(1)
        self.start_node(2)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        sync_blocks(self.nodes[0:3])
        self.nodes[0].resendwallettransactions()
        sync_mempools(self.nodes[0:2])

        self.nodes[1].generate(1)
        sync_blocks(self.nodes[0:3])
        node_2_bal += 2

        #tx should be added to balance because after restarting the nodes tx should be broadcastet
        assert_equal(self.nodes[2].getbalance(), node_2_bal)

        #send a tx with value in a string (PR#6380 +)
        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "2")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-2'))

        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "0.0001")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-0.0001'))

        #check if JSON parser can handle scientific notation in strings
        txId  = self.nodes[0].sendtoaddress(self.nodes[2].getnewaddress(), "1e-4")
        txObj = self.nodes[0].gettransaction(txId)
        assert_equal(txObj['amount'], Decimal('-0.0001'))

        # This will raise an exception because the amount type is wrong
        assert_raises_rpc_error(-3, "Invalid amount", self.nodes[0].sendtoaddress, self.nodes[2].getnewaddress(), "1f-4")

        # This will raise an exception since generate does not accept a string
        assert_raises_rpc_error(-1, "not an integer", self.nodes[0].generate, "2")

        # Import address and private key to check correct behavior of spendable unspents
        # 1. Send some coins to generate new UTXO
        address_to_import = self.nodes[2].getnewaddress()
        txid = self.nodes[0].sendtoaddress(address_to_import, 1)
        sync_mempools(self.nodes[0:2])
        self.nodes[1].generate(1)
        self.sync_all([self.nodes[0:3]])

        # 2. Import address from node2 to node1
        self.nodes[1].importaddress(address_to_import)

        # 3. Validate that the imported address is watch-only on node1
        assert self.nodes[1].validateaddress(address_to_import)["iswatchonly"]

        # 4. Check that the unspents after import are not spendable
        assert_array_result(self.nodes[1].listunspent(),
                           {"address": address_to_import},
                           {"spendable": False})

        # 5. Import private key of the previously imported address on node1
        priv_key = self.nodes[2].dumpprivkey(address_to_import)
        self.nodes[1].importprivkey(priv_key)

        # 6. Check that the unspents are now spendable on node1
        assert_array_result(self.nodes[1].listunspent(),
                           {"address": address_to_import},
                           {"spendable": True})

        # Mine a block from node1 to an address from node2
        cbAddr = self.nodes[2].getnewaddress()
        blkHash = self.nodes[1].generatetoaddress(1, cbAddr)[0]
        cbTxId = self.nodes[1].getblock(blkHash)['tx'][0]
        self.sync_all([self.nodes[0:3]])

        # Check that the txid and balance is found by node2
        self.nodes[2].gettransaction(cbTxId)

        # check if wallet or blockchain maintenance changes the balance
        self.sync_all([self.nodes[0:3]])
        blocks = self.nodes[1].generatetoaddress(2, self.nodes[1].getnewaddress('', 'bech32'))
        self.sync_all([self.nodes[0:3]])
        balance_nodes = [self.nodes[i].getbalance() for i in range(3)]
        block_count = self.nodes[1].getblockcount()

        # Check modes:
        #   - True: unicode escaped as \u....
        #   - False: unicode directly as UTF-8
        for mode in [True, False]:
            self.nodes[0].ensure_ascii = mode
            # unicode check: Basic Multilingual Plane, Supplementary Plane respectively
            for s in [u'рыба', u'𝅘𝅥𝅯']:
                addr = self.nodes[0].getaccountaddress(s)
                label = self.nodes[0].getaccount(addr)
                assert_equal(label, s)
                assert s in self.nodes[0].listaccounts().keys()
        self.nodes[0].ensure_ascii = True # restore to default

        # maintenance tests
        maintenance = [
            '-rescan',
            '-reindex',
            '-zapwallettxes=1',
            '-zapwallettxes=2',
            # disabled until issue is fixed: https://github.com/bitcoin/bitcoin/issues/7463
            # '-salvagewallet',
        ]
        chainlimit = 6
        for m in maintenance:
            self.log.info("check " + m)
            self.stop_nodes()
            # set lower ancestor limit for later
            self.start_node(0, [m, "-limitancestorcount="+str(chainlimit)])
            self.start_node(1, [m, "-limitancestorcount="+str(chainlimit)])
            self.start_node(2, [m, "-limitancestorcount="+str(chainlimit)])
            if m == '-reindex':
                # reindex will leave rpc warm up "early"; Wait for it to finish
                wait_until(lambda: [block_count] * 3 == [self.nodes[i].getblockcount() for i in range(3)])
            assert_equal(balance_nodes, [self.nodes[i].getbalance() for i in range(3)])

        # Exercise listsinceblock with the last two blocks
        coinbase_tx_1 = self.nodes[1].listsinceblock(blocks[0])
        assert_equal(coinbase_tx_1["lastblock"], blocks[1])
        transactions = set(map(lambda x: x["txid"], coinbase_tx_1["transactions"]))
        assert_equal(len(transactions), 1)
        assert_equal(coinbase_tx_1["transactions"][0]["blockhash"], blocks[1])
        assert_equal(len(self.nodes[1].listsinceblock(blocks[1])["transactions"]), 0)

        connect_nodes_bi(self.nodes, 0, 1)

        # ==Check that wallet prefers to use coins that don't exceed mempool limits =====

        # Get all non-zero utxos together
        chain_addrs = [self.nodes[0].getnewaddress(), self.nodes[0].getnewaddress()]
        singletxid = self.nodes[0].sendtoaddress(chain_addrs[0], self.nodes[0].getbalance(), "", "", True)
        node0_balance = self.nodes[0].getbalance()
        # Split into two chains
        rawtx = self.nodes[0].createrawtransaction([{"txid":singletxid, "vout":0}], {chain_addrs[0]:node0_balance/2-Decimal('0.01'), chain_addrs[1]:node0_balance/2-Decimal('0.01')})
        signedtx = self.nodes[0].signrawtransaction(rawtx)
        self.nodes[0].sendrawtransaction(signedtx["hex"])
        sync_mempools(self.nodes[:2])
        self.nodes[1].generate(1)
        sync_blocks(self.nodes[:2])

        # Make a long chain of unconfirmed payments without hitting mempool limit
        # Each tx we make leaves only one output of change on a chain 1 longer
        # Since the amount to send is always much less than the outputs, we only ever need one output
        # So we should be able to generate exactly chainlimit txs for each original output
        sending_addr = self.nodes[1].getnewaddress()
        txid_list = []
        for i in range(chainlimit*2):
            txid_list.append(self.nodes[0].sendtoaddress(sending_addr, Decimal('0.0001')))
        assert_equal(self.nodes[0].getmempoolinfo()['size'], chainlimit*2)
        assert_equal(len(txid_list), chainlimit*2)

        # Without walletrejectlongchains, we will still generate a txid
        # The tx will be stored in the wallet but not accepted to the mempool
        extra_txid = self.nodes[0].sendtoaddress(sending_addr, Decimal('0.0001'))
        assert extra_txid not in self.nodes[0].getrawmempool()
        assert extra_txid in [tx["txid"] for tx in self.nodes[0].listtransactions()]
        self.nodes[0].abandontransaction(extra_txid)
        total_txs = len(self.nodes[0].listtransactions("*",99999))

        # Try with walletrejectlongchains
        # Double chain limit but require combining inputs, so we pass SelectCoinsMinConf
        self.stop_node(0)
        self.start_node(0, extra_args=["-walletrejectlongchains", "-limitancestorcount="+str(2*chainlimit)])

        # wait for loadmempool
        timeout = 10
        while (timeout > 0 and len(self.nodes[0].getrawmempool()) < chainlimit*2):
            time.sleep(0.5)
            timeout -= 0.5
        assert_equal(len(self.nodes[0].getrawmempool()), chainlimit*2)

        node0_balance = self.nodes[0].getbalance()
        # With walletrejectlongchains we will not create the tx and store it in our wallet.
        assert_raises_rpc_error(-4, "Transaction has too long of a mempool chain", self.nodes[0].sendtoaddress, sending_addr, node0_balance, "", "", True)

        # Verify nothing new in wallet
        assert_equal(total_txs, len(self.nodes[0].listtransactions("*",99999)))

if __name__ == '__main__':
    WalletTest().main()
