#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import time
from random import randint
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework



# class EsperanzaTest(UnitETestFramework):
#
# def set_test_params(self):
#     self.num_nodes = 4
#
#     params_data = {
#         'epochLength': 2,
#     }
#     json_params = json.dumps(params_data)
#
#     validator_node_params = [
#         '-validating=1',
#         '-staking=0',
#         '-debug=all',
#         '-esperanzaconfig=' + json_params
#     ]
#     proposer_node_params = ['-staking=0','-debug=all', '-esperanzaconfig='+json_params]
#
#     self.extra_args = [validator_node_params,
#                        proposer_node_params,
#                        proposer_node_params,
#                        proposer_node_params]
#     self.setup_clean_chain = True
#
# def run_test(self):
#
#     nodes = self.nodes
#
#     block_time = 1
#     main_node = nodes[0]
#
#     self.create_wallet()
#     main_node.extkeyimportmaster('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
#     nodes[1].extkeyimportmaster('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
#     nodes[2].extkeyimportmaster('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
#     nodes[3].extkeyimportmaster('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')
#     assert (nodes[0].getwalletinfo()['total_balance'] == 10000)
#     assert (nodes[1].getwalletinfo()['total_balance'] == 10000)
#     assert (nodes[2].getwalletinfo()['total_balance'] == 10000)
#     assert (nodes[3].getwalletinfo()['total_balance'] == 10000)
#
#     nodes[0].createdeposit("", 10000)
#
#     # mine some blocks to allow the deposit to get included in a block
#     for n in range(0, 5):
#         nodes[(n % 3) + 1].generate(1)
#         time.sleep(block_time)
#
#     time.sleep(1)
#
#     resp = main_node.getvalidatorinfo()
#     assert resp["enabled"]
#     assert_equal(resp["validator_status"], "IS_VALIDATING")
#
#     print("Test succeeded.")
#
#     return
#
# def propose_block(self):
#     selected_node = randint(0, self.num_nodes - 1)
#     self.nodes[selected_node].generate(1)
#
# def create_wallet(self):
#     oRoot0 = self.nodes[0].mnemonic('new')
#
#     ro = self.nodes[0].extkey('import', oRoot0['master'], 'import save key', 'true')
#     self.nodes[0].extkey('setMaster', ro['id'])
#
#     ro = self.nodes[0].extkey('deriveAccount', '1st account')
#     self.nodes[0].extkey('setDefaultAccount', ro['account'])

# if __name__ == '__main__':
#     EsperanzaTest().main()
