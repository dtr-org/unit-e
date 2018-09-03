#!/usr/bin/env python3

from test_framework.test_framework import UnitETestFramework


class SpendGenensisTest(UnitETestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        node = self.nodes[0]
        block = node.getblock(node.getblockhash(0))
        assert (block['hash'])

        node.importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        node.rescanblockchain(0, 0) #UNIT-E this can be removed as soon as importmasterkey correclty perform a rescan

        # Make the coinbase mature
        node.generate(100)
        assert (node.getwalletinfo()['balance'] == 10000)

        # Send a tx to yourself spending the tx
        node.sendtoaddress(node.getnewaddress(), 5000)


if __name__ == '__main__':
    SpendGenensisTest().main()
