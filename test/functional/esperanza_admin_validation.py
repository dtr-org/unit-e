#!/ usr / bin / env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import re

from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import *
from test_framework.mininode import *


class TestNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.reject_map = {}

    def on_reject(self, message):
        txid = "%064x" % message.data
        reason = "%s" % message.reason
        self.reject_map[txid] = reason


def create_end_tx(node, txid, vout, address, change_amount):
    end_perm_data = "0300"  # CommandType + EmptyPayload

    inputs = [
        {
            "txid": txid,
            "vout": vout
        }
    ]

    outputs = {
        address: change_amount,
        "data": end_perm_data
    }

    return node.createrawtransaction(inputs, outputs)


def set_type_to_admin(raw_tx):
    tx = CTransaction()
    f = BytesIO(hex_str_to_bytes(raw_tx))
    tx.deserialize(f)

    tx.nVersion = (tx.nVersion & 0x0000FFFF) | (ADMIN_TX_TYPE << 16)

    return bytes_to_hex_str(tx.serialize())


class AdminValidation(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ['-proposing=1', '-debug=all', '-whitelist=127.0.0.1']]
        self.setup_clean_chain = True

    def send_end_via_mininode(self, address):
        funds_tx = self.admin.sendtoaddress(address, Decimal("1"))
        self.wait_for_transaction(funds_tx, timeout=10)
        _, n = Admin.find_output_for_address(self.admin, funds_tx, address)

        raw_tx = create_end_tx(self.admin, funds_tx, n, address,
                               Decimal("0.09"))

        raw_tx = set_type_to_admin(raw_tx)

        raw_tx = self.admin.signrawtransaction(raw_tx)["hex"]

        tx = CTransaction()
        f = BytesIO(hex_str_to_bytes(raw_tx))
        tx.deserialize(f)

        self.admin.p2p.send_and_ping(msg_witness_tx(tx))

        tx.rehash()
        txid = tx.hash

        return txid

    def reject(self, address, expected_reason):
        txid = self.send_end_via_mininode(address)

        reason = self.reject_tracker.reject_map.get(txid, None)
        if reason is None:
            raise AssertionError("Tx was not rejected!")
        assert_equal(expected_reason, reason)

    def accept(self, address):
        txid = self.send_end_via_mininode(address)

        if txid in self.reject_tracker.reject_map:
            raise AssertionError("Tx was rejected!")

    def create_new_multisig(self, n_signatures, n_keys, address_type):
        addresses = list(
            self.admin.getnewaddress("", address_type) for _ in range(n_keys))

        return self.admin.addmultisigaddress(n_signatures, addresses, "",
                                             address_type)["address"]

    def create_admin_multisig(self, n_signatures, n_keys, address_type):
        addresses = self.admin_addresses[0:n_keys]

        return self.admin.addmultisigaddress(n_signatures, addresses, "",
                                             address_type)["address"]

    def run_test(self):
        self.admin = self.nodes[0]
        self.reject_tracker = TestNode()
        self.admin.add_p2p_connection(self.reject_tracker)
        network_thread_start()

        self.admin.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        assert_equal(10000, self.admin.getbalance())

        # Waiting for maturity
        self.admin.generate(120)
        self.sync_all()

        self.admin.p2p.wait_for_verack()

        self.reject(self.admin.getnewaddress("", "legacy"),
                    "b'admin-invalid-script-sig'")
        self.reject(self.admin.getnewaddress("", "p2sh-segwit"),
                    "b'admin-invalid-script-sig'")
        self.reject(self.admin.getnewaddress("", "bech32"),
                    "b'admin-invalid-script-sig'")
        self.reject(self.create_new_multisig(2, 3, "p2sh-segwit"),
                    "b'admin-not-authorized'")

        self.admin_addresses = Admin.import_admin_keys(self.admin)

        self.reject(self.admin_addresses[0], "b'admin-invalid-script-sig'")
        self.reject(self.create_admin_multisig(2, 3, "legacy"),
                    "b'admin-invalid-script-sig'")
        self.reject(self.create_admin_multisig(2, 3, "bech32"),
                    "b'admin-invalid-script-sig'")
        self.reject(self.create_admin_multisig(2, 2, "p2sh-segwit"),
                    "b'admin-invalid-witness'")

        self.accept(self.create_admin_multisig(2, 3, "p2sh-segwit"))

        print("Test succeeded.")


if __name__ == '__main__':
    AdminValidation().main()
