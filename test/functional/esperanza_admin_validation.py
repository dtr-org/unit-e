#!/usr/bin/env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

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


def create_tx(cmds, node, txid, vout, address, change_amount):
    inputs = [
        {
            "txid": txid,
            "vout": vout
        }
    ]

    outputs = {
        address: change_amount,
    }

    for i in range(len(cmds)):
        key = "data" + str(i)
        outputs[key] = cmds[i]

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

    def send_end_via_mininode(self, cmds, address):
        funds_tx = self.admin.sendtoaddress(address, Decimal("1"))
        self.wait_for_transaction(funds_tx, timeout=10)
        _, n = Admin.find_output_for_address(self.admin, funds_tx, address)

        raw_tx = create_tx(cmds, self.admin, funds_tx, n, address,
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

    def rejects(self, cmds, address, expected_reason):
        txid = self.send_end_via_mininode(cmds, address)

        reason = self.reject_tracker.reject_map.get(txid, None)
        if reason is None:
            raise AssertionError("Tx was not rejected!")
        assert_equal(expected_reason, reason)

    def accepts(self, cmds, address):
        txid = self.send_end_via_mininode(cmds, address)

        reason = self.reject_tracker.reject_map.get(txid, None)
        if reason is not None:
            raise AssertionError("Tx was rejected: " + reason)

    def create_new_multisig(self, n_signatures, n_keys, address_type):
        addresses = list(
            self.admin.getnewaddress("", address_type) for _ in range(n_keys))

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

        end_permissioning_cmd = "0300"

        self.rejects([end_permissioning_cmd],
                     self.admin.getnewaddress("", "legacy"),
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.admin.getnewaddress("", "p2sh-segwit"),
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.admin.getnewaddress("", "bech32"),
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.create_new_multisig(2, 3, "p2sh-segwit"),
                     "b'admin-not-authorized'")

        admin_addresses = Admin.import_admin_keys(self.admin)

        self.rejects([end_permissioning_cmd],
                     admin_addresses[0],
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.admin.addmultisigaddress(2, admin_addresses[0:2], "",
                                                   "legacy")["address"],
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.admin.addmultisigaddress(2, admin_addresses, "",
                                                   "bech32")["address"],
                     "b'admin-invalid-script-sig'")
        self.rejects([end_permissioning_cmd],
                     self.admin.addmultisigaddress(2, admin_addresses[0:2], "",
                                                   "p2sh-segwit")["address"],
                     "b'admin-invalid-witness'")

        admin_address = self.admin.addmultisigaddress(2, admin_addresses, "",
                                                      "p2sh-segwit")["address"]

        self.rejects([],
                     admin_address,
                     "b'admin-no-commands'")

        self.rejects([end_permissioning_cmd, end_permissioning_cmd],
                     admin_address,
                     "b'admin-double-disable'")

        garbage_cmds = ["05", "98478754", "0005", "020100"]

        self.rejects(garbage_cmds,
                     admin_address,
                     "b'admin-no-commands'")

        new_addresses = list(self.admin.getnewaddress() for _ in range(3))
        new_pubkeys = list(
            self.admin.validateaddress(address)["pubkey"] for address in
            new_addresses)

        # RESET_ADMINS + 3 (pubkeys) + (0x21, pubkey) + ...
        reset_admin_cmd = "0203" + \
                          "21" + new_pubkeys[0] + \
                          "21" + new_pubkeys[1] + \
                          "21" + new_pubkeys[2]

        self.accepts([reset_admin_cmd], admin_address)

        # Admin commands have no power until included into block
        self.admin.generate(1)

        self.rejects([end_permissioning_cmd],
                     admin_address,
                     "b'admin-not-authorized'")

        admin_address = self.admin.addmultisigaddress(2, new_addresses, "",
                                                      "p2sh-segwit")["address"]

        self.accepts([end_permissioning_cmd], admin_address)

        print("Test succeeded.")


if __name__ == '__main__':
    AdminValidation().main()
