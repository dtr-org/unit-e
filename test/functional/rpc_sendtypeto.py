#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import *
from test_framework.address import *

class SendtypetoTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.enable_mocktime()

    def run_test(self):
        assert_raises_rpc_error(-3, 'Not an array', self.nodes[0].sendtypeto, 'unite', 'unite', 'out')

        outputs = [{'address': 'foo', 'amount': 1}]
        assert_raises_rpc_error(-5, 'Invalid UnitE address', self.nodes[0].sendtypeto, 'unite', 'unite', outputs)

        outputs = [{'address': '2NBYu3St3rtzJb8AzvraXnXCUxEuCs23eZo', 'amount': self.nodes[0].getbalance() + 1}]
        assert_raises_rpc_error(-6, 'Insufficient funds', self.nodes[0].sendtypeto, 'unite', 'unite', outputs)

        # Multiple outputs
        node_1_address = self.nodes[1].getnewaddress()
        outputs = [
            {'address': node_1_address, 'amount': 0.5},
            {'address': '2NBYu3St3rtzJb8AzvraXnXCUxEuCs23eZo', 'amount': 0.25}]
        tx1_id = self.nodes[0].sendtypeto('unite', 'unite', outputs, 'comment', 'comment-to')
        tx = self.nodes[0].getrawtransaction(tx1_id, True)
        assert_equal(len(tx['vout']), 3)
        vout1 = next(o for o in tx['vout'] if node_1_address in o['scriptPubKey']['addresses'])
        assert_equal(vout1['value'], Decimal(0.5))

        # Subtract fee from amount
        outputs = [{'address': node_1_address, 'amount': 0.5, 'subfee': True}]
        tx2_id = self.nodes[0].sendtypeto('unite', 'unite', outputs, 'comment', 'comment-to')
        estimate_fee = True
        fees = self.nodes[0].sendtypeto('unite', 'unite', outputs, 'comment', 'comment-to', estimate_fee)
        tx = self.nodes[0].getrawtransaction(tx2_id, True)
        vout2 = next(o for o in tx['vout'] if node_1_address in o['scriptPubKey']['addresses'])
        assert_equal(vout2['value'], Decimal(0.5) - fees['fee'])

        self.sync_all()
        self.nodes[1].generate(6)

        # coincontrol: inputs
        outputs = [{'address': '2NBYu3St3rtzJb8AzvraXnXCUxEuCs23eZo', 'amount': 0.6}]
        coin_control = {'inputs': [{'tx': tx1_id, 'n': vout1['n']}, {'tx': tx2_id, 'n': vout2['n']}]}
        estimate_fee = False
        tx3_id = self.nodes[1].sendtypeto(
            'unite', 'unite', outputs, 'comment', 'comment-to', estimate_fee, coin_control)
        tx = self.nodes[1].getrawtransaction(tx3_id, True)
        assert_equal(len(tx['vin']), 2)
        assert_array_result(tx['vin'], {'txid': tx1_id}, {'vout': vout1['n']})
        assert_array_result(tx['vin'], {'txid': tx2_id}, {'vout': vout2['n']})

        # coincontrol: changeaddress
        change_address = self.nodes[1].getrawchangeaddress()
        coin_control = {'changeaddress': change_address}
        estimate_fee = False
        tx4_id = self.nodes[1].sendtypeto(
            'unite', 'unite', outputs, 'comment', 'comment-to', estimate_fee, coin_control)
        tx = self.nodes[1].getrawtransaction(tx4_id, True)
        assert_equal(len(tx['vout']), 2)
        next(o for o in tx['vout'] if change_address in o['scriptPubKey']['addresses'])

        # Estimate fee
        estimate_fee = True
        fee_per_byte = Decimal('0.001') / 1000
        self.nodes[0].settxfee(fee_per_byte * 1000)
        fees = self.nodes[0].sendtypeto('unite', 'unite', outputs, 'comment', 'comment-to', estimate_fee)
        assert_equal(fees['fee'], fees['bytes'] * fee_per_byte)

        fee_per_byte *= 2
        coin_control = {'fee_rate': fee_per_byte * 1000}
        fees = self.nodes[0].sendtypeto('unite', 'unite', outputs, 'comment', 'comment-to', estimate_fee, coin_control)
        assert_equal(fees['fee'], fees['bytes'] * fee_per_byte)


if __name__ == "__main__":
    SendtypetoTest().main()
