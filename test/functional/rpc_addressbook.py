#!/usr/bin/env python3
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import UnitETestFramework
from test_framework.util import *


class AddressBookRPCTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test (self):
        node = self.nodes[0]

        # filteraddresses [offset] [count] [sort_code] [match_str] [match_owned]
        resp = node.filteraddresses(0, 100, 0, '', 0)
        assert_equal(len(resp), 0)

        resp = node.addressbookinfo()
        assert_equal(resp['total'], 0)
        assert_equal(resp['num_receive'], 0)
        assert_equal(resp['num_send'], 0)

        assert_raises_rpc_error(
            -8, 'is not in the address book',
            node.manageaddressbook, 'edit', '2NAcZN89Jb9HUF7hLiy965DMEZ6Pk5ntKvC', 'newLabel')

        assert_raises_rpc_error(
            -8, 'is not in the address book',
            node.manageaddressbook, 'del', '2NAcZN89Jb9HUF7hLiy965DMEZ6Pk5ntKvC')

        not_owned_address = '2MyLtWsxJAVvAkW7m2AGiGeZGsrAQEf4hCf'
        resp = node.manageaddressbook('add', not_owned_address, 'label1', 'purpose1')
        assert_equal(resp['result'], 'success')
        assert_equal(resp['address'], not_owned_address)
        assert_equal(resp['label'], 'label1')
        assert_equal(resp['purpose'], 'purpose1')

        resp = node.filteraddresses(0, 100, 0, '', 2)  # list not owned
        assert_equal(len(resp), 1)
        assert_equal(resp[0]['address'], not_owned_address)
        assert_equal(resp[0]['label'], 'label1')
        assert_equal(resp[0]['owned'], False)

        resp = node.manageaddressbook('edit', not_owned_address, 'label2', 'purpose2')
        assert_equal(resp['label'], 'label2')
        assert_equal(resp['purpose'], 'purpose2')
        assert_equal(resp['owned'], False)

        resp = node.manageaddressbook('info', not_owned_address)
        assert_equal(resp['address'], not_owned_address)
        assert_equal(resp['label'], 'label2')
        assert_equal(resp['purpose'], 'purpose2')
        assert_equal(resp['owned'], False)

        resp = node.filteraddresses(0, 100, 0, '', 2)
        assert_equal(len(resp), 1)
        assert_equal(resp[0]['address'], not_owned_address)
        assert_equal(resp[0]['label'], 'label2')

        # Owned addresses
        owned_address = node.getnewaddress()
        assert_raises_rpc_error(
            -8, 'is recorded in the address book',
            node.manageaddressbook, 'add', owned_address, 'owned1', 'receive')

        resp = node.filteraddresses(0, 100, 0, '', 1)  # list only owned
        assert_equal(len(resp), 1)
        assert_equal(resp[0]['address'], owned_address)
        assert_equal(resp[0]['owned'], True)

        resp = node.manageaddressbook('edit', owned_address, 'owned2', 'receive2')
        assert_equal(resp['result'], 'success')
        assert_equal(resp['owned'], True)

        resp = node.manageaddressbook('info', owned_address)
        assert_equal(resp['address'], owned_address)
        assert_equal(resp['label'], 'owned2')
        assert_equal(resp['purpose'], 'receive2')
        assert_equal(resp['owned'], True)

        resp = node.filteraddresses(0, 100)
        assert_equal(len(resp), 2)

        resp = node.addressbookinfo()
        assert_equal(resp['total'], 2)
        assert_equal(resp['num_receive'], 1)
        assert_equal(resp['num_send'], 1)

        resp = node.manageaddressbook('del', not_owned_address)
        assert_equal(resp['address'], not_owned_address)

        resp = node.filteraddresses(0, 100)
        assert_equal(len(resp), 1)


if __name__ == '__main__':
    AddressBookRPCTest().main()
