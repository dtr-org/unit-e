#!/ usr / bin / env python3
# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.util import *
from test_framework.test_framework import UnitETestFramework
from test_framework.admin import *


class AdminAuthorizationTest(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.extra_args = [
            ['-proposing=1', '-debug=all'],
            ['-proposing=0', '-debug=all'],
            ['-proposing=0', '-debug=all']]
        self.setup_clean_chain = True

    def create_unauthorized_admin(self, admin, proposer):
        addresses = [admin.getnewaddress("", "p2sh-segwit"),
                     admin.getnewaddress("", "p2sh-segwit"),
                     admin.getnewaddress("", "p2sh-segwit")]

        address = admin.addmultisigaddress(2, addresses, "", "p2sh-segwit")[
            "address"]

        pubkeys = list(admin.validateaddress(a)["pubkey"] for a in addresses)

        return Admin(self, admin, proposer, address, Decimal("10")), pubkeys

    def run_test(self):
        proposer = self.nodes[0]
        admin_node1 = self.nodes[1]
        admin_node2 = self.nodes[1]

        proposer.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        assert_equal(10000, proposer.getbalance())

        # Waiting for maturity
        proposer.generate(120)
        self.sync_all()

        # admin1 - authorized, admin2 - not
        admin1 = Admin.authorize(self, admin_node1, proposer)
        admin1.assert_last_op_ok()

        admin2, admin2_pubkeys = self.create_unauthorized_admin(admin_node2,
                                                                proposer)
        admin2.assert_last_op_ok()

        command = {
            "cmd": "reset_admins",
            "payload": admin2_pubkeys
        }

        admin2.send([command])
        admin2.assert_last_op_is_rpc_error(-26)

        admin1.send([command])
        admin1.assert_last_op_ok()

        # admin1 - not authorized, admin2 - authorized
        command = {"cmd": "end_permissioning"}

        admin1.send([command])
        admin1.assert_last_op_is_rpc_error(-26)

        admin2.send([command])
        admin2.assert_last_op_ok()

        # permissioning ended
        admin1.send([command])
        admin1.assert_last_op_is_rpc_error(-26)

        admin2.send([command])
        admin2.assert_last_op_is_rpc_error(-26)

        print("Test succeeded.")


if __name__ == '__main__':
    AdminAuthorizationTest().main()
