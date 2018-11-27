#!/usr/bin/env python3

# Copyright (c) 2018 The Unit-e core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from asyncio import (
    AbstractEventLoop,
    Protocol,
    Task,
    Transport,
    coroutine,
    gather,
    sleep as asyncio_sleep
)
from logging import getLogger
from struct import pack, unpack

from test_framework.messages import hash256
from test_framework.util import p2p_port


MSG_HEADER_LENGTH = 4 + 12 + 4 + 4


logger = getLogger('TestFramework.nodes_hub')


class NodesHub:
    """
    A central hub to connect all the nodes at test/simulation time. It has many purposes:
      - Give us the ability to capture & analyze traffic
      - Give us the ability to add arbitrary delays/latencies between any node

    The hub will open many ports at the same time to handle inbound connections, one for each node. When a node A wants
    to send a message to node B, the message will travel through the hub (H hereinafter). So, if A wants to be connected
    to B and C, it will establish two connections to H (every open port of H will represent a specific node), and H will
    establish one new connection to B, and another one to C.

    In this class, we refer to the nodes through their index in the self.nodes property.
    """

    def __init__(self, loop, nodes, host='127.0.0.1', sync_setup=False):
        self.loop = loop  # type: AbstractEventLoop
        self.nodes = nodes

        self.host = host

        self.node2node_delays = {}  # This allows us to specify asymmetric delays

        self.proxy_coroutines = []
        self.proxy_tasks = []
        self.sender2proxy_transports = {}

        self.relay_tasks = {}
        self.proxy2receiver_transports = {}

        self.pending_connection = None  # Lock-like object used by NodesHub.connect_nodes

        self.setup_proxies(sync_setup)

    def setup_proxies(self, sync_setup=False):
        """
        This method creates a listener proxy for each node, the connections from each proxy to the real node that they
        represent will be done whenever a node connects to the proxy.

        If sync_setup=False, then the connections are scheduled, but will be performed only when we start the loop.
        """

        for i, node in enumerate(self.nodes):
            proxy_coroutine = self.loop.create_server(
                protocol_factory=lambda: NodeProxy(hub_ref=self),
                host=self.host,
                port=self.get_proxy_port(i)
            )

            if not sync_setup:
                self.proxy_tasks.append(self.loop.create_task(proxy_coroutine))
            else:
                self.proxy_coroutines.append(proxy_coroutine)

    def sync_start_proxies(self):
        """
        Helper to make easier using NodesHub in non-asyncio aware code. Not directly executed in constructor to ensure
        decoupling between constructive & behavioral patterns.

        It starts the nodes's proxies.
        """
        self.loop.run_until_complete(gather(*self.proxy_coroutines))

    def sync_biconnect_nodes_as_linked_list(self, nodes_list=None):
        """
        Helper to make easier using NodesHub in non-asyncio aware code.
        Connects nodes as a linked list.
        """
        if nodes_list is None:
            nodes_list = range(len(self.nodes))

        if 0 == len(nodes_list):
            return

        connection_futures = []

        for i, j in zip(nodes_list, nodes_list[1:]):
            connection_futures.append(self.connect_nodes(i, j))
            connection_futures.append(self.connect_nodes(j, i))

        self.loop.run_until_complete(gather(*connection_futures))

    def get_node_port(self, node_idx):
        return p2p_port(node_idx)

    def get_proxy_port(self, node_idx):
        return p2p_port(len(self.nodes) + 1 + node_idx)

    def get_proxy_address(self, node_idx):
        return '%s:%s' % (self.host, self.get_proxy_port(node_idx))

    def set_nodes_delay(self, outbound_idx, inbound_idx, delay):
        if delay == 0:
            self.node2node_delays.pop((outbound_idx, inbound_idx), None)
        else:
            self.node2node_delays[(outbound_idx, inbound_idx)] = delay  # delay is measured in seconds

    def disconnect_nodes(self, outbound_idx, inbound_idx):
        sender2proxy_transport = self.sender2proxy_transports.get((outbound_idx, inbound_idx), None)  # type: Transport
        proxy2receiver_transport = self.proxy2receiver_transports.get((outbound_idx, inbound_idx), None)  # type: Transport
        relay_task = self.relay_tasks.get((outbound_idx, inbound_idx), None)  # type: Task

        if sender2proxy_transport is not None and not sender2proxy_transport.is_closing():
            sender2proxy_transport.close()

        if proxy2receiver_transport is not None and not proxy2receiver_transport.is_closing():
            proxy2receiver_transport.close()

        if relay_task is not None and not relay_task.cancelled():
            relay_task.cancel()

        # Removing references
        self.sender2proxy_transports.pop((outbound_idx, inbound_idx), None)
        self.proxy2receiver_transports.pop((outbound_idx, inbound_idx), None)
        self.relay_tasks.pop((outbound_idx, inbound_idx), None)

    @coroutine
    def connect_nodes(self, outbound_idx: int, inbound_idx: int):
        """
        :param outbound_idx: It refers to the "sender" (the one asking for a new connection)
        :param inbound_idx: It refers to the "receiver" (the one listening for new connections)
        """

        # We have to wait until all the proxies are configured and listening
        while len(self.proxy_tasks) + len(self.proxy_coroutines) < len(self.nodes):
            yield from asyncio_sleep(0)

        # We have to be sure that all the previous calls to connect_nodes have finished. Because we are using
        # cooperative scheduling we don't have to worry about race conditions, this while loop is enough.
        while self.pending_connection is not None:
            yield from asyncio_sleep(0)

        # We acquire the lock. This tuple is also useful for the NodeProxy instance.
        self.pending_connection = (outbound_idx, inbound_idx)

        if (
                self.pending_connection in self.sender2proxy_transports or
                self.pending_connection in self.proxy2receiver_transports
        ):
            raise RuntimeError('Connection (node%s --> node%s) already established' % self.pending_connection[:])

        self.connect_sender_to_proxy(*self.pending_connection)  # 1st step: connect "sender" to the proxy
        self.connect_proxy_to_receiver(*self.pending_connection)  # 2nd step: connect proxy 2 its associated node

        # We wait until we know that all the connections have been properly created
        while (
                self.pending_connection not in self.sender2proxy_transports or
                self.pending_connection not in self.proxy2receiver_transports
        ):
            yield from asyncio_sleep(0)

        self.pending_connection = None  # We release the lock

    def connect_sender_to_proxy(self, outbound_idx, inbound_idx):
        """
        Establishes a connection between a real node and the proxy representing another node
        """
        sender_node = self.nodes[outbound_idx]
        proxy_address = self.get_proxy_address(inbound_idx)

        sender_node.addnode(proxy_address, 'add')     # Add the proxy to the outgoing connections list
        sender_node.addnode(proxy_address, 'onetry')  # Connect to the proxy. Will trigger NodeProxy.connection_made

    def connect_proxy_to_receiver(self, outbound_idx, inbound_idx):
        """
        Creates a sender that connects to a node and relays messages between that node and its associated proxy
        """

        relay_coroutine = self.loop.create_connection(
            protocol_factory=lambda c2sp=self.pending_connection: ProxyRelay(hub_ref=self, sender2receiver_pair=c2sp),
            host=self.host,
            port=self.get_node_port(inbound_idx)
        )
        self.relay_tasks[(outbound_idx, inbound_idx)] = self.loop.create_task(relay_coroutine)


class NodeProxy(Protocol):
    def __init__(self, hub_ref):
        self.hub_ref = hub_ref
        self.sender2receiver_pair = None
        self.recvbuf = b''

    def connection_made(self, transport: Transport):
        self.sender2receiver_pair = self.hub_ref.pending_connection

        logger.debug('Client %s connected to proxy %s' % self.sender2receiver_pair[:])
        self.hub_ref.sender2proxy_transports[self.sender2receiver_pair] = transport

    def connection_lost(self, exc):
        logger.debug('Lost connection between sender %s and proxy %s' % self.sender2receiver_pair[:])
        self.hub_ref.disconnect_nodes(*self.sender2receiver_pair)

    def data_received(self, data):
        self.hub_ref.loop.create_task(self.__handle_received_data(data))

    @coroutine
    def __handle_received_data(self, data):
        while self.sender2receiver_pair not in self.hub_ref.proxy2receiver_transports:
            yield from asyncio_sleep(0)  # We can't relay the data yet, we need a connection on the other side

        if self.sender2receiver_pair in self.hub_ref.node2node_delays:
            yield from asyncio_sleep(self.hub_ref.node2node_delays[self.sender2receiver_pair])

        if len(data) > 0:
            logger.debug(
                'Proxy connection %s received %s bytes' % (repr(self.sender2receiver_pair), len(data))
            )
            self.recvbuf += data
            self.recvbuf = process_buffer(
                node_port=self.hub_ref.get_proxy_port(self.sender2receiver_pair[0]),
                buffer=self.recvbuf,
                transport=self.hub_ref.proxy2receiver_transports[self.sender2receiver_pair]
            )


class ProxyRelay(Protocol):
    def __init__(self, hub_ref, sender2receiver_pair):
        self.hub_ref = hub_ref
        self.sender2receiver_pair = sender2receiver_pair
        self.receiver2sender_pair = sender2receiver_pair[::-1]
        self.recvbuf = b''

    def connection_made(self, transport: Transport):
        logger.debug(
            'Created connection between proxy and its associated node %s to receive messages from node %s' %
            self.receiver2sender_pair
        )
        self.hub_ref.proxy2receiver_transports[self.sender2receiver_pair] = transport

    def connection_lost(self, exc):
        logger.debug(
            'Lost connection between proxy and its associated node %s to receive messages from node %s' %
            self.receiver2sender_pair
        )
        self.hub_ref.disconnect_nodes(*self.sender2receiver_pair)

    def data_received(self, data):
        self.hub_ref.loop.create_task(self.__handle_received_data(data))

    @coroutine
    def __handle_received_data(self, data):
        while self.sender2receiver_pair not in self.hub_ref.sender2proxy_transports:
            yield from asyncio_sleep(0)  # We can't relay the data yet, we need a connection on the other side

        if self.receiver2sender_pair in self.hub_ref.node2node_delays:
            yield from asyncio_sleep(self.hub_ref.node2node_delays[self.receiver2sender_pair])

        if len(data) > 0:
            logger.debug(
                'Proxy relay connection %s received %s bytes' % (repr(self.sender2receiver_pair), len(data))
            )
            self.recvbuf += data
            self.recvbuf = process_buffer(
                node_port=self.hub_ref.get_proxy_port(self.sender2receiver_pair[1]),
                buffer=self.recvbuf,
                transport=self.hub_ref.sender2proxy_transports[self.sender2receiver_pair]
            )


def process_buffer(node_port, buffer, transport: Transport):
    """
    This function helps the hub to impersonate nodes by modifying 'version' messages changing the "from" addresses.
    """
    while len(buffer) > MSG_HEADER_LENGTH:  # We do nothing until we have (magic + command + length + checksum)

        # We only care about command & msglen, but not about messages correctness.
        msglen = unpack("<i", buffer[4 + 12:4 + 12 + 4])[0]

        # We wait until we have the full message
        if len(buffer) < MSG_HEADER_LENGTH + msglen:
            return

        command = buffer[4:4 + 12].split(b'\x00', 1)[0]
        logger.debug('Processing command %s' % str(command))

        if b'version' == command:
            msg = buffer[MSG_HEADER_LENGTH:MSG_HEADER_LENGTH + msglen]
            msg = (
                    msg[:4 + 8 + 8 + 26 + (4 + 8 + 16)] +
                    pack('!H', node_port) +  # Injecting the proxy's port info
                    msg[4 + 8 + 8 + 26 + (4 + 8 + 16 + 2):]
            )

            msg_checksum = hash256(msg)[:4]  # That's a truncated double sha256
            new_header = buffer[:MSG_HEADER_LENGTH - 4] + msg_checksum

            transport.write(new_header + msg)
        else:
            # We pass an unaltered message
            transport.write(buffer[:MSG_HEADER_LENGTH + msglen])

        buffer = buffer[MSG_HEADER_LENGTH + msglen:]

    return buffer
