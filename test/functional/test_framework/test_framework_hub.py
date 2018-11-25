#!/usr/bin/env python3

# Copyright (c) 2018 The unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from abc import ABCMeta
from asyncio import (
    AbstractEventLoop,
    get_event_loop
)
from concurrent.futures import ThreadPoolExecutor

from test_framework.nodes_hub import NodesHub
from test_framework.test_framework import UnitETestFramework


class UnitEHubTestFramework(UnitETestFramework, metaclass=ABCMeta):
    def __init__(self, loop: AbstractEventLoop = None):
        super().__init__()

        if loop is None:
            loop = get_event_loop()

        self.loop = loop
        self.hub: NodesHub = None

        # Black magic to hide asyncio & threading details to test implementers
        self.__original_run_test = self.run_test
        self.run_test = self.__run_test_wrapper

    def setup_network(self):
        """Override this method to customize test network topology"""
        self.setup_nodes()

        self.hub = NodesHub(loop=self.loop, nodes=self.nodes, sync_setup=True)
        self.hub.sync_start_proxies()
        self.hub.sync_biconnect_nodes_as_linked_list()

        self.sync_all()

    def __run_test_wrapper(self):
        """Run tests code in a separated non-blocking thread for simplicity"""
        self.loop.set_default_executor(ThreadPoolExecutor())
        task = self.loop.run_in_executor(None, self.__original_run_test)
        self.loop.run_until_complete(task)
