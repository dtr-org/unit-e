#!/ usr / bin / env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal
from test_framework.authproxy import JSONRPCException


class Admin:
    @staticmethod
    def authorize(framework, admin_node, donor_node=None,
                  amount=Decimal("10")):

        private_keys = ["cQzPd94MUUE6Gtoue6Y86S7apaLwJA223f4Md3GiaX7j7vGDPDXp",
                        "cVYxRyHk6B9x3pgnUz1vEkvVpNhCiCvtTbYDLy4EzW8PNKRyHNvG",
                        "cRFymdYKDZpDEkZLeR4WNbk7hiZtnG3nuWH4uyxwtKyJdT54vf9b"]

        addresses = ["n1Rf25dBYJ2PAMUx5FbAWmRVHt1CkAS6Vj",
                     "mwEynwmJ2XEQ6sm2U7ji7yLaEUhbkRCZx2",
                     "mjqD9Fc81DjCgNfJqLkvrXUVYqygCxhabm"]

        for key in private_keys:
            admin_node.importprivkey(key)

        address = admin_node.addmultisigaddress(2, addresses, "",
                                                "p2sh-segwit")["address"]

        if donor_node is None:
            donor_node = admin_node

        return Admin(framework, admin_node, donor_node, address, amount)

    def send(self, commands, fee=Decimal("0.001")):
        self.last_exception = None

        if self.prevout is None:
            raise AssertionError("No prevout")

        try:
            txid = self.admin_node.sendadmincommands([self.prevout], fee,
                                                     commands, self.address)
            self.framework.wait_for_transaction(txid)
            self.prevout = self.find_prevout(txid)
            self.donor_node.generate(1)
            self.framework.sync_all()
            return txid
        except Exception as exception:
            self.last_exception = exception

    @staticmethod
    def authorize_and_disable(framework, admin_node, donor_node=None,
                              fee=Decimal("0.001")):
        admin = Admin.authorize(framework, admin_node, donor_node, fee)
        admin.assert_last_op_ok()
        admin.send([{'cmd': 'end_permissioning'}], fee)
        admin.assert_last_op_ok()

    def find_prevout(self, txid):
        raw_tx = self.admin_node.getrawtransaction(txid)
        tx = self.admin_node.decoderawtransaction(raw_tx)

        for out in tx["vout"]:
            script_pub_key = out["scriptPubKey"]

            if script_pub_key["asm"].startswith("OP_RETURN"):
                continue

            addresses = script_pub_key["addresses"]

            if len(addresses) != 1 or addresses[0] != self.address:
                continue

            return txid, out["n"]

        return None

    def __init__(self, framework, admin_node, donor_node, address, amount):
        super().__init__()

        if donor_node is None:
            donor_node = admin_node

        self.admin_node = admin_node
        self.donor_node = donor_node
        self.address = address
        self.framework = framework

        self.prevout = None
        self.last_exception = None

        try:
            txid = donor_node.sendtoaddress(address, amount)
            framework.wait_for_transaction(txid, timeout=10)

            self.prevout = self.find_prevout(txid)
        except Exception as e:
            self.last_exception = e

    def assert_last_op_ok(self):
        if self.last_exception is not None:
            raise AssertionError(self.last_exception)

    def assert_last_op_is_rpc_error(self, error_code):
        if self.last_exception is None:
            raise AssertionError("Exception expected")
        if not isinstance(self.last_exception, JSONRPCException):
            raise AssertionError(self.last_exception)
        if error_code != self.last_exception.error["code"]:
            raise AssertionError(
                "Expected error code: %s, actual: %s, message: '%s'" %
                (error_code, self.last_exception.error["code"],
                 self.last_exception.error["message"]))
