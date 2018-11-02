#!/usr/bin/env python3

from test_framework.mininode import network_thread_start, P2PInterface
from test_framework.blocktools import *
from test_framework.key import CECKey
from test_framework.test_framework import UnitETestFramework
from test_framework.util import assert_equal
import subprocess
import sys
import time
import json

class P2P(P2PInterface):
    def __init__(self, test):
        super().__init__()
        self.test = test

    def on_message(self, message):
        print("<", message);
        super().on_message(message)

    def send_message(self, message, pushbuf=False):
        print(">", message)
        super().send_message(message, pushbuf)

    def on_inv(self, message):
        for inv in message.inv:
            if inv.type == 2 or inv.type == 2 | (1 << 30): # MSG_BLOCK or MSG_WITNESS_BLOCK
                self.test.on_block_accepted(inv.hash)

    def on_reject(self, message):
        self.test.on_block_rejected(message.data)

    def on_getdata(self, message):
        for inv in message.inv:
            if inv.type == 2 or inv.type == 2 | (1 << 30): # MSG_BLOCK or MSG_WITNESS_BLOCK
                self.test.on_block_requested(inv.hash)


# Check that node rejects blocks which are behind the current last finalized epoch
class FinalizationForkChoiceBase(UnitETestFramework):
    def __init__(self):
        super().__init__()
        self.blocks = {}
        self.accepted = set()
        self.rejected = set()
        self.requested = set()

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [[ '-printtoconsole', '-debug=all', '-whitelist=127.0.0.1', '-esperanzaconfig={"epochLength": 5}']];
        self.setup_clean_chain = True

    def getbestblockhash(self):
        return int(self.nodes[0].getbestblockhash(), 16)

    def getesperanzastate(self):
        return self.nodes[0].getesperanzastate()

    def on_block_accepted(self, hash):
        self.accepted.add(hash)

    def on_block_rejected(self, hash):
        self.rejected.add(hash)

    def on_block_requested(self, hash):
        self.requested.add(hash)

    def wait_accepted(self, hash, timeout=5):
        if not timeout is None:
            stop_at = time.time() + timeout
        while True:
            if hash in self.accepted:
                return
            elif hash in self.rejected:
                raise RuntimeError("Block {0} rejected".format(hash))
            time.sleep(0.1)
            if not timeout is None and time.time() >= stop_at:
                raise RuntimeError("Block {0} hasn't been accepted".format(hash))

    def wait_rejected(self, hash, timeout=5, inverse=False):
        if not timeout is None:
            stop_at = time.time() + timeout
        while True:
            if hash in self.accepted:
                if inverse:
                    return
                else:
                    raise RuntimeError("Block {0} accepted".format(hash))
            elif hash in self.rejected:
                if inverse:
                    raise RuntimeError("Block {0} rejected".format(hash))
                else:
                    return
            if not timeout is None:
                time.sleep(0.1)
                if time.time() >= stop_at:
                    if inverse:
                        return
                    else:
                        raise RuntimeError("Block {0} hasn't been rejected".format(hash))

    def wait_requested(self, hash, timeout=5):
        if not timeout is None:
            stop_at = time.time() + timeout
        while True:
            if hash in self.requested:
                return
            time.sleep(0.1)
            if not timeout is None and time.time() >= stop_at:
                raise RuntimeError("Block {0} hasn't been requested".format(hash))


    def wait_not_rejected(self, hash, timeout=1):
        return self.wait_rejected(hash, timeout=timeout, inverse=True)

    def check_esperanza(self, current_epoch=None, current_dynasty=None, last_finalized_epoch=None, last_justified_epoch=None, validators=None):
        state = self.getesperanzastate()
        def check(v, expected):
            if not v is None:
                assert_equal(v, expected)
        check(current_epoch, state['currentEpoch'])
        check(current_dynasty, state['currentDynasty'])
        check(last_finalized_epoch, state['lastFinalizedEpoch'])
        check(last_justified_epoch, state['lastJustifiedEpoch'])
        check(validators, state['validators'])

    def create_block(self, prev=None, secret=None):
        if secret is None:
            secret = "default"
        coinbase_key = CECKey()
        coinbase_key.set_secretbytes(bytes(secret, "utf-8"))
        coinbase_pubkey = coinbase_key.get_pubkey()
        if prev is None:
            block_base_hash = self.getbestblockhash()
            block_time = int(time.time()) + 1
        else:
            block_base_hash = prev.sha256
            block_time = prev.nTime + 1
        height = prev.height + 1 if prev else 1
        coinbase = create_coinbase(height, coinbase_pubkey)
        coinbase.rehash()
        b = create_block(block_base_hash, coinbase, block_time)
        b.solve()
        b.height = height
        return b

    def send_message(self, msg):
        self.nodes[0].p2p.send_message(msg, True)

    def send_block(self, block):
        self.blocks[block.sha256] = block
        self.send_message(msg_block(block))
        return block

    def send_header(self, block):
        self.send_message(msg_headers([block]))
        return block


class FinalizationForkChoiceBasic(FinalizationForkChoiceBase):
    def run_test(self):
        block = self.create_block
        send = self.send_block
        accepted = self.wait_accepted
        rejected = self.wait_rejected
        not_rejected = self.wait_not_rejected
        self.nodes[0].add_p2p_connection(P2P(self))
        network_thread_start()
        self.nodes[0].p2p.wait_for_verack()
        # chain a
        a = [None] * 100
        tip = a[0]

        for x in range(1, 15):
            tip = a[x] = send(block(a[x - 1]))
        accepted(tip.sha256)
        self.check_esperanza(current_epoch=2, last_finalized_epoch=1, last_justified_epoch=1)

        # do not reject...
        tmp = send(block(a[9], "tmp"))
        not_rejected(tmp.sha256) # it's actually silently rejected if node hasn't explicitly requested this block

        tip = a[15] = send(block(a[14]))
        accepted(tip.sha256)
        self.check_esperanza(current_epoch=3, last_finalized_epoch=2, last_justified_epoch=2)

        # we're in a new epoch, reject
        tmp = send(block(a[9], "tmp2"))
        rejected(tmp.sha256)

        self.nodes[0].p2p.disconnect_node()


if __name__ == '__main__':
    FinalizationForkChoiceBasic().main()
