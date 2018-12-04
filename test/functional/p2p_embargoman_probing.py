#!/usr/bin/env python3
# Copyright(c) 2018 The unit - e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http: // www.opensource.org/licenses/mit-license.php.
"""
Performs probing attack embargoed transactions.

Embargo some transaction and connect two mininodes to the source.
The first mininode will be probing for the above transaction.
Other will be probing for a transaction that does not exist. We then compare
responses we received and they should be the same
"""

from test_framework.util import connect_nodes, assert_equal
from test_framework.test_framework import UnitETestFramework
from test_framework.mininode import P2PInterface, network_thread_start, \
    msg_getdata, msg_mempool
from test_framework.messages import CInv
import time
import threading

# Minimum embargo.
EMBARGO_SECONDS = 60

# Perform probing from the moment tx was sent and until PROBE_UNTIL_SECONDS
# have passed
PROBE_UNTIL_SECONDS = 40


class EmbargoProbing(UnitETestFramework):
    def set_test_params(self):
        self.num_nodes = 2

        self.extra_args = [['-proposing=1',
                            '-debug=all',
                            '-embargotxs=1',
                            '-embargomin=%s' % EMBARGO_SECONDS]] * self.num_nodes
        self.setup_clean_chain = True

    def setup_network(self):
        super().setup_nodes()
        connect_nodes(self.nodes[0], 1)

    def run_test(self):
        source = self.nodes[0]
        source.importmasterkey(
            'swap fog boost power mountain pair gallery crush price fiscal '
            'thing supreme chimney drastic grab acquire any cube cereal '
            'another jump what drastic ready')

        # Exit IBD
        source.generate(1)
        self.sync_all()

        start_time = time.perf_counter()
        real_tx = source.sendtoaddress(self.nodes[1].getnewaddress(), 1)
        imaginary_tx = "424fe97567d21b6ae7f821b6b3af2519478a9f3def1e0bcb6dd0560eacfb0241"

        real_probe = Probe(real_tx)
        imaginary_probe = Probe(imaginary_tx)

        source.add_p2p_connection(real_probe)
        source.add_p2p_connection(imaginary_probe)
        network_thread_start()

        real_probe.wait_for_verack()
        imaginary_probe.wait_for_verack()

        # Finally start probing for some time
        while time.perf_counter() < start_time + PROBE_UNTIL_SECONDS:
            real_probe.get_data()
            imaginary_probe.get_data()

            real_probe.get_mempool()
            imaginary_probe.get_mempool()

            time.sleep(1)

        while True:
            try:
                check_during_embargo(real_probe, imaginary_probe)
                break
            except AssertionError:
                if time.perf_counter() - start_time >= EMBARGO_SECONDS:
                    raise
                time.sleep(1)

        while True:
            try:
                real_probe.get_data()
                check_after_embargo(real_probe)
                break
            except AssertionError:
                if time.perf_counter() - start_time - EMBARGO_SECONDS > 10:
                    raise
                time.sleep(1)


class Probe(P2PInterface):
    def __init__(self, tx):
        super().__init__()
        self.target_hash = int(tx, 16)

        self.lock = threading.Lock()

        self.not_found_count = 0
        self.target_received = False
        self.target_inved = False

    def get_data(self):
        want = msg_getdata()
        want.inv.append(CInv(1, self.target_hash))
        self.send_message(want)

    def get_mempool(self):
        self.send_message(msg_mempool())

    def on_tx(self, message):
        with self.lock:
            message.tx.calc_sha256()
            incoming_hash = message.tx.sha256
            assert_equal(type(incoming_hash), type(self.target_hash))
            if incoming_hash == self.target_hash:
                self.target_received = True

    def on_notfound(self, message):
        with self.lock:
            for inv in message.inv:
                assert_equal(type(inv.hash), type(self.target_hash))
                if inv.hash == self.target_hash:
                    self.not_found_count += 1

    def on_inv(self, message):
        with self.lock:
            for inv in message.inv:
                assert_equal(type(inv.hash), type(self.target_hash))
                if inv.hash == self.target_hash:
                    self.target_inved = True


def check_during_embargo(real_probe, imaginary_probe):
    with real_probe.lock:
        with imaginary_probe.lock:
            assert (not imaginary_probe.target_received)
            assert (imaginary_probe.not_found_count != 0)

            assert_equal(real_probe.not_found_count,
                         imaginary_probe.not_found_count)

            assert_equal(real_probe.target_received,
                         imaginary_probe.target_received)

            assert_equal(real_probe.target_inved,
                         imaginary_probe.target_inved)


def check_after_embargo(real_probe):
    with real_probe.lock:
        assert (real_probe.target_inved)
        assert (real_probe.target_received)


if __name__ == '__main__':
    EmbargoProbing().main()
