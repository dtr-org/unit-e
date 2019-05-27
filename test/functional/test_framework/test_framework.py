#!/usr/bin/env python3
# Copyright (c) 2014-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

import configparser
from enum import Enum
import logging
import json
import argparse
import os
import pdb
import shutil
import sys
import tempfile
import time
from decimal import Decimal

from .authproxy import JSONRPCException
from . import coverage
from .regtest_mnemonics import regtest_mnemonics
from .test_node import TestNode
from .mininode import NetworkThread
from .util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    check_json_precision,
    cleanup_datadir,
    connect_nodes_bi,
    disconnect_nodes,
    get_datadir_path,
    initialize_datadir,
    p2p_port,
    sync_blocks,
    sync_mempools,
    connect_nodes,
    wait_until,
)
from .messages import (
    FromHex,
    CTransaction,
    TxType,
)


class TestStatus(Enum):
    PASSED = 1
    FAILED = 2
    SKIPPED = 3

TEST_EXIT_PASSED = 0
TEST_EXIT_FAILED = 1
TEST_EXIT_SKIPPED = 77

COINBASE_MATURITY = 100  # Should match the value from consensus.h
STAKE_SPLIT_THRESHOLD = 1000  # Should match the value from blockchain_parameters.cpp
PROPOSER_REWARD = Decimal('3.75')  # Will not decrease as tests don't generate enough blocks

TMPDIR_PREFIX = "unite_func_test_"


class SkipTest(Exception):
    """This exception is raised to skip a test"""

    def __init__(self, message):
        self.message = message

# This parameter simulates the scenario that the node "never" reaches finalization.
# The purpose of it is to adapt Bitcoin tests to Unit-e which contradict with the finalization
# so the existing tests will perform the check within one dynasty.
# When this parameter is used, framework must be configured with `setup_clean_chain = True`
# to prevent using the cache which was generated with the finalization enabled.
DISABLE_FINALIZATION = '-esperanzaconfig={"epochLength": 99999}'

class UnitETestMetaClass(type):
    """Metaclass for UnitETestFramework.

    Ensures that any attempt to register a subclass of `UnitETestFramework`
    adheres to a standard whereby the subclass overrides `set_test_params` and
    `run_test` but DOES NOT override either `__init__` or `main`. If any of
    those standards are violated, a ``TypeError`` is raised."""

    def __new__(cls, clsname, bases, dct):
        if not clsname == 'UnitETestFramework':
            if not ('run_test' in dct and 'set_test_params' in dct):
                raise TypeError("UnitETestFramework subclasses must override "
                                "'run_test' and 'set_test_params'")
            if '__init__' in dct or 'main' in dct:
                raise TypeError("UnitETestFramework subclasses may not override "
                                "'__init__' or 'main'")

        return super().__new__(cls, clsname, bases, dct)


class UnitETestFramework(metaclass=UnitETestMetaClass):
    """Base class for a unite test script.

    Individual unite test scripts should subclass this class and override the set_test_params() and run_test() methods.

    Individual tests can also override the following methods to customize the test setup:

    - add_options()
    - setup_chain()
    - setup_network()
    - setup_nodes()

    The __init__() and main() methods should not be overridden.

    This class also contains various public and private helper methods."""

    def __init__(self):
        """Sets test framework defaults. Do not override this method. Instead, override the set_test_params() method"""
        self.setup_clean_chain = False
        self.nodes = []
        self.network_thread = None
        self.rpc_timeout = 60  # Wait for up to 60 seconds for the RPC server to respond
        self.supports_cli = False
        self.bind_to_localhost_only = True
        self.set_test_params()

        assert hasattr(self, "num_nodes"), "Test must set self.num_nodes in set_test_params()"

        if hasattr(self, "customchainparams"):
            if not hasattr(self, "extra_args"):
                self.extra_args = []
            for i in range(len(self.extra_args), self.num_nodes):
                self.extra_args.append([])
            for i in range(0, self.num_nodes):
                if i < len(self.customchainparams):
                    json_value = json.dumps(self.customchainparams[i])
                    self.extra_args[i].append("-customchainparams=" + json_value)

    def main(self):
        """Main function. This should not be overridden by the subclass test scripts."""

        parser = argparse.ArgumentParser(usage="%(prog)s [options]")
        parser.add_argument("--nocleanup", dest="nocleanup", default=False, action="store_true",
                            help="Leave unit-e daemons and test.* datadir on exit or error")
        parser.add_argument("--noshutdown", dest="noshutdown", default=False, action="store_true",
                            help="Don't stop unit-e daemons after the test execution")
        parser.add_argument("--cachedir", dest="cachedir", default=os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/../../cache"),
                            help="Directory for caching pregenerated datadirs (default: %(default)s)")
        parser.add_argument("--tmpdir", dest="tmpdir", help="Root directory for datadirs")
        parser.add_argument("-l", "--loglevel", dest="loglevel", default="INFO",
                            help="log events at this level and higher to the console. Can be set to DEBUG, INFO, WARNING, ERROR or CRITICAL. Passing --loglevel DEBUG will output all logs to console. Note that logs at all levels are always written to the test_framework.log file in the temporary test directory.")
        parser.add_argument("--tracerpc", dest="trace_rpc", default=False, action="store_true",
                            help="Print out all RPC calls as they are made")
        parser.add_argument("--portseed", dest="port_seed", default=os.getpid(), type=int,
                            help="The seed to use for assigning port numbers (default: current process id)")
        parser.add_argument("--coveragedir", dest="coveragedir",
                            help="Write tested RPC commands into this directory")
        parser.add_argument("--configfile", dest="configfile",
                            default=os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/../../config.ini"),
                            help="Location of the test framework config file (default: %(default)s)")
        parser.add_argument("--pdbonfailure", dest="pdbonfailure", default=False, action="store_true",
                            help="Attach a python debugger if test fails")
        parser.add_argument("--usecli", dest="usecli", default=False, action="store_true",
                            help="use unit-e-cli instead of RPC for all commands")
        parser.add_argument("--perf", dest="perf", default=False, action="store_true",
                            help="profile running nodes with perf for the duration of the test")
        self.add_options(parser)
        self.options = parser.parse_args()

        PortSeed.n = self.options.port_seed

        check_json_precision()

        self.options.cachedir = os.path.abspath(self.options.cachedir)

        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))
        self.config = config
        self.options.unit_e = os.getenv("UNIT_E", default=config["environment"]["BUILDDIR"] + '/src/unit-e' + config["environment"]["EXEEXT"])
        self.options.unit_e_cli = os.getenv("UNIT_E_CLI", default=config["environment"]["BUILDDIR"] + '/src/unit-e-cli' + config["environment"]["EXEEXT"])

        os.environ['PATH'] = os.pathsep.join([
            os.path.join(config['environment']['BUILDDIR'], 'src'),
            os.path.join(config['environment']['BUILDDIR'], 'src', 'qt'),
            os.environ['PATH']
        ])

        # Set up temp directory and start logging
        if self.options.tmpdir:
            self.options.tmpdir = os.path.abspath(self.options.tmpdir)
            os.makedirs(self.options.tmpdir, exist_ok=False)
        else:
            self.options.tmpdir = tempfile.mkdtemp(prefix=TMPDIR_PREFIX)
        self._start_logging()

        self.log.debug('Setting up network thread')
        self.network_thread = NetworkThread()
        self.network_thread.start()

        success = TestStatus.FAILED

        try:
            if self.options.usecli:
                if not self.supports_cli:
                    raise SkipTest("--usecli specified but test does not support using CLI")
                self.skip_if_no_cli()
            self.skip_test_if_missing_module()
            self.setup_chain()
            self.setup_network()
            self.run_test()
            success = TestStatus.PASSED
        except JSONRPCException as e:
            self.log.exception("JSONRPC error")
        except SkipTest as e:
            self.log.warning("Test Skipped: %s" % e.message)
            success = TestStatus.SKIPPED
        except AssertionError as e:
            self.log.exception("Assertion failed")
        except KeyError as e:
            self.log.exception("Key error")
        except Exception as e:
            self.log.exception("Unexpected exception caught during testing")
        except KeyboardInterrupt as e:
            self.log.warning("Exiting after keyboard interrupt")

        if success == TestStatus.FAILED and self.options.pdbonfailure:
            print("Testcase failed. Attaching python debugger. Enter ? for help")
            pdb.set_trace()

        self.log.debug('Closing down network thread')
        self.network_thread.close()
        if not self.options.noshutdown:
            self.log.info("Stopping nodes")
            if self.nodes:
                self.stop_nodes()
        else:
            for node in self.nodes:
                node.cleanup_on_exit = False
            self.log.info("Note: unit-e daemons were not stopped and may still be running")

        should_clean_up = (
            not self.options.nocleanup and
            not self.options.noshutdown and
            success != TestStatus.FAILED and
            not self.options.perf
        )
        if should_clean_up:
            self.log.info("Cleaning up {} on exit".format(self.options.tmpdir))
            cleanup_tree_on_exit = True
        elif self.options.perf:
            self.log.warning("Not cleaning up dir {} due to perf data".format(self.options.tmpdir))
            cleanup_tree_on_exit = False
        else:
            self.log.warning("Not cleaning up dir {}".format(self.options.tmpdir))
            cleanup_tree_on_exit = False

        if success == TestStatus.PASSED:
            self.log.info("Tests successful")
            exit_code = TEST_EXIT_PASSED
        elif success == TestStatus.SKIPPED:
            self.log.info("Test skipped")
            exit_code = TEST_EXIT_SKIPPED
        else:
            self.log.error("Test failed. Test logging available at %s/test_framework.log", self.options.tmpdir)
            self.log.error("Hint: Call {} '{}' to consolidate all logs".format(os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../combine_logs.py"), self.options.tmpdir))
            exit_code = TEST_EXIT_FAILED
        logging.shutdown()
        if cleanup_tree_on_exit:
            shutil.rmtree(self.options.tmpdir)
        sys.exit(exit_code)

    # Methods to override in subclass test scripts.
    def set_test_params(self):
        """Tests must this method to change default values for number of nodes, topology, etc"""
        raise NotImplementedError

    def add_options(self, parser):
        """Override this method to add command-line options to the test"""
        pass

    def skip_test_if_missing_module(self):
        """Override this method to skip a test if a module is not compiled"""
        pass

    def setup_chain(self):
        """Override this method to customize blockchain setup"""
        self.log.info("Initializing test directory " + self.options.tmpdir)
        self.log.info("Debug file at " + self.options.tmpdir + "/node0/regtest/debug.log")
        if self.setup_clean_chain:
            self._initialize_chain_clean()
        else:
            self._initialize_chain()

    def setup_network(self):
        """Override this method to customize test network topology"""
        self.setup_nodes()

        # Connect the nodes as a "chain".  This allows us
        # to split the network between nodes 1 and 2 to get
        # two halves that can work on competing chains.
        for i in range(self.num_nodes - 1):
            connect_nodes_bi(self.nodes, i, i + 1)
        self.sync_all()

    def setup_nodes(self):
        """Override this method to customize test node setup"""
        extra_args = None
        if hasattr(self, "extra_args"):
            extra_args = self.extra_args
        self.add_nodes(self.num_nodes, extra_args)
        self.start_nodes()
        self.import_deterministic_coinbase_privkeys()
        if not self.setup_clean_chain:
            for n in self.nodes:
                assert_equal(n.getblockchaininfo()["blocks"], 199)
            # To ensure that all nodes are out of IBD, the most recent block
            # must have a timestamp not too old (see IsInitialBlockDownload()).
            self.log.debug('Generate a block with current time')
            block_hash = self.nodes[0].generate(1)[0]
            block = self.nodes[0].getblock(blockhash=block_hash, verbosity=0)
            for n in self.nodes:
                n.submitblock(block)
                chain_info = n.getblockchaininfo()
                assert_equal(chain_info["blocks"], 200)

    def setup_stake_coins(self, *args, rescan=True, offset=0):
        for i, node in enumerate(args):
            node.mnemonics = regtest_mnemonics[i + 2]['mnemonics']
            node.initial_stake = regtest_mnemonics[i + 2]['balance']
            node.importmasterkey(node.mnemonics, "", rescan)

    def import_deterministic_coinbase_privkeys(self):
        for n in self.nodes:
            wallets = n.listwallets()
            w = n.get_wallet_rpc(wallets[0])
            try:
                w.getwalletinfo()
            except JSONRPCException as e:
                assert str(e).startswith('Method not found')
                continue

            w.importprivkey(privkey=n.get_deterministic_priv_key().key, label='coinbase')

    def run_test(self):
        """Tests must override this method to define test logic"""
        raise NotImplementedError

    # Public helper methods. These can be accessed by the subclass test scripts.

    def add_nodes(self, num_nodes, extra_args=None, *, rpchost=None, binary=None):
        """Instantiate TestNode objects.

        Should only be called once after the nodes have been specified in
        set_test_params()."""
        if self.bind_to_localhost_only:
            extra_confs = [["bind=127.0.0.1"]] * num_nodes
        else:
            extra_confs = [[]] * num_nodes
        if extra_args is None:
            extra_args = [[]] * num_nodes
        if binary is None:
            binary = [self.options.unit_e] * num_nodes
        assert_equal(len(extra_confs), num_nodes)
        assert_equal(len(extra_args), num_nodes)
        assert_equal(len(binary), num_nodes)
        for i in range(num_nodes):
            print("Starting node " + str(i) + " with args: " + ' '.join(str(e) for e in extra_args[i]))
            self.nodes.append(TestNode(
                i,
                get_datadir_path(self.options.tmpdir, i),
                rpchost=rpchost,
                timewait=self.rpc_timeout,
                unit_e=binary[i],
                unit_e_cli=self.options.unit_e_cli,
                coverage_dir=self.options.coveragedir,
                cwd=self.options.tmpdir,
                extra_conf=extra_confs[i],
                extra_args=extra_args[i],
                use_cli=self.options.usecli,
                start_perf=self.options.perf,
            ))

    def start_node(self, i, *args, **kwargs):
        """Start a unit-e"""

        node = self.nodes[i]

        node.start(*args, **kwargs)
        node.wait_for_rpc_connection()

        if self.options.coveragedir is not None:
            coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def start_nodes(self, extra_args=None, *args, **kwargs):
        """Start multiple unit-e daemons"""

        if extra_args is None:
            extra_args = [None] * self.num_nodes
        assert_equal(len(extra_args), self.num_nodes)
        try:
            for i, node in enumerate(self.nodes):
                node.start(extra_args[i], *args, **kwargs)
            for node in self.nodes:
                node.wait_for_rpc_connection()
        except:
            # If one node failed to start, stop the others
            self.stop_nodes()
            raise

        if self.options.coveragedir is not None:
            for node in self.nodes:
                coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def stop_node(self, i, expected_stderr='', wait=0):
        """Stop a unit-e test node"""
        self.nodes[i].stop_node(expected_stderr, wait=wait)
        self.nodes[i].wait_until_stopped()

    def stop_nodes(self, wait=0):
        """Stop multiple unit-e test nodes"""
        for node in self.nodes:
            # Issue RPC to stop nodes
            node.stop_node(wait=wait)

        for node in self.nodes:
            # Wait for nodes to stop
            node.wait_until_stopped()

    def restart_node(self, i, extra_args=None, cleanup=False):
        """Stop and start a test node"""
        self.stop_node(i)
        if cleanup:
            cleanup_datadir(self.options.tmpdir, i)
            initialize_datadir(self.options.tmpdir, i)
        self.start_node(i, extra_args)

    def wait_for_node_exit(self, i, timeout):
        self.nodes[i].process.wait(timeout)

    def wait_for_transaction(self, txid, timeout=150, nodes=None):
        if nodes is None:
            nodes = self.nodes
        timeout += time.perf_counter()

        presence = dict.fromkeys(range(len(nodes)))

        while time.perf_counter() < timeout:
            all_have = True
            for node in nodes:
                try:
                    if presence[node.index]:
                        continue
                    node.getrawtransaction(txid)
                    presence[node.index] = True
                except JSONRPCException:
                    presence[node.index] = False
                    all_have = False

            if all_have:
                return

            time.sleep(0.1)

        raise RuntimeError('Failed to wait for transaction %s. Presence: %s'
                           % (txid, presence))

    def split_network(self):
        """
        Split the network of four nodes into nodes 0/1 and 2/3.
        """
        disconnect_nodes(self.nodes[1], 2)
        disconnect_nodes(self.nodes[2], 1)
        self.sync_all([self.nodes[:2], self.nodes[2:]])

    def join_network(self):
        """
        Join the (previously split) network halves together.
        """
        connect_nodes_bi(self.nodes, 1, 2)
        self.sync_all()

    def sync_all(self, node_groups=None):
        if not node_groups:
            node_groups = [self.nodes]

        for group in node_groups:
            sync_blocks(group)
            sync_mempools(group)

    @staticmethod
    def wait_for_vote_and_disconnect(finalizer, node):
        """
        Wait until the finalizer votes on the node's tip
        and disconnect the finalizer from the node.
        """
        def connected(addr):
            for p in finalizer.getpeerinfo():
                if p['addr'] == addr:
                    return True
            return False

        def wait_for_new_vote(old_txs):
            try:
                wait_until(lambda: len(node.getrawmempool()) > len(old_txs), timeout=10)
            except AssertionError as e:
                msg = "{}\nERROR: finalizer did not vote for the tip={} during {} sec.".format(
                    e, node.getblockcount(), 10)
                raise AssertionError(msg)

        ip_port = "127.0.0.1:" + str(p2p_port(node.index))
        assert not connected(ip_port), 'finalizer must not be connected for the correctness of the test'

        txs = node.getrawmempool()
        connect_nodes(finalizer, node.index)
        assert connected(ip_port)  # ensure that the right IP was used

        sync_blocks([finalizer, node])
        wait_for_new_vote(txs)
        disconnect_nodes(finalizer, node.index)

        new_txs = [tx for tx in node.getrawmempool() if tx not in txs]
        assert_equal(len(new_txs), 1)
        raw_vote = node.getrawtransaction(new_txs[0])
        vote = FromHex(CTransaction(), raw_vote)
        assert_equal(vote.get_type(), TxType.VOTE)
        return raw_vote

    def generate_sync(self, generator_node, nblocks=1, nodes=None):
        """
        Generates nblocks on a given node. Performing full sync after each block
        """
        if nodes is None:
            nodes = self.nodes
        generated_blocks = []
        for _ in range(nblocks):
            block = generator_node.generate(1)[0]
            generated_blocks.append(block)
            sync_blocks(nodes)

            # VoteIfNeeded is called on a background thread.
            # By syncing we ensure that all votes will be in the mempools at the
            # end of this iteration
            for node in nodes:
                node.syncwithvalidationinterfacequeue()

            sync_mempools(nodes)

        return generated_blocks

    @classmethod
    def generate_epoch(cls, proposer, finalizer, count=1):
        """
        Generate `count` epochs and collect votes.
        """
        epoch_length = proposer.getfinalizationconfig()['epochLength']
        assert epoch_length > 1
        votes=[]
        for _ in range(count):
            proposer.generatetoaddress(epoch_length - 1, proposer.getnewaddress('', 'bech32'))
            cls.wait_for_vote_and_disconnect(finalizer, proposer)
            for tx in proposer.getrawmempool():
                tx = FromHex(CTransaction(), proposer.getrawtransaction(tx))
                if tx.get_type() == TxType.VOTE:
                    votes.append(tx)
            proposer.generatetoaddress(1, proposer.getnewaddress('', 'bech32'))
        return votes

    # Private helper methods. These should not be accessed by the subclass test scripts.

    def _start_logging(self):
        # Add logger and logging handlers
        self.log = logging.getLogger('TestFramework')
        self.log.setLevel(logging.DEBUG)
        # Create file handler to log all messages
        fh = logging.FileHandler(self.options.tmpdir + '/test_framework.log', encoding='utf-8')
        fh.setLevel(logging.DEBUG)
        # Create console handler to log messages to stderr. By default this logs only error messages, but can be configured with --loglevel.
        ch = logging.StreamHandler(sys.stdout)
        # User can provide log level as a number or string (eg DEBUG). loglevel was caught as a string, so try to convert it to an int
        ll = int(self.options.loglevel) if self.options.loglevel.isdigit() else self.options.loglevel.upper()
        ch.setLevel(ll)
        # Format logs the same as unit-e's debug.log with microprecision (so log files can be concatenated and sorted)
        formatter = logging.Formatter(fmt='%(asctime)s.%(msecs)03d000Z %(name)s (%(levelname)s): %(message)s', datefmt='%Y-%m-%dT%H:%M:%S')
        formatter.converter = time.gmtime
        fh.setFormatter(formatter)
        ch.setFormatter(formatter)
        # add the handlers to the logger
        self.log.addHandler(fh)
        self.log.addHandler(ch)

        if self.options.trace_rpc:
            rpc_logger = logging.getLogger("UnitERPC")
            rpc_logger.setLevel(logging.DEBUG)
            rpc_handler = logging.StreamHandler(sys.stdout)
            rpc_handler.setLevel(logging.DEBUG)
            rpc_logger.addHandler(rpc_handler)

    def _initialize_chain(self):
        """Initialize a pre-mined blockchain for use by the test.

        Create a cache of a 199-block-long chain (with wallet) for MAX_NODES
        Afterward, create num_nodes copies from the cache."""

        assert self.num_nodes <= MAX_NODES
        create_cache = False
        for i in range(MAX_NODES):
            if not os.path.isdir(get_datadir_path(self.options.cachedir, i)):
                create_cache = True
                break

        if create_cache:
            self.log.debug("Creating data directories from cached datadir")

            # find and delete old cache directories if any exist
            for i in range(MAX_NODES):
                if os.path.isdir(get_datadir_path(self.options.cachedir, i)):
                    shutil.rmtree(get_datadir_path(self.options.cachedir, i))

            # Create cache directories, run unit-e daemons:
            for i in range(MAX_NODES):
                datadir = initialize_datadir(self.options.cachedir, i)
                args = [self.options.unit_e, "-datadir=" + datadir, '-disablewallet']
                if i > 0:
                    args.append("-connect=127.0.0.1:" + str(p2p_port(0)))
                self.nodes.append(TestNode(
                    i,
                    get_datadir_path(self.options.cachedir, i),
                    extra_conf=["bind=127.0.0.1"],
                    extra_args=[],
                    rpchost=None,
                    timewait=self.rpc_timeout,
                    unit_e=self.options.unit_e,
                    unit_e_cli=self.options.unit_e_cli,
                    coverage_dir=None,
                    cwd=self.options.tmpdir,
                ))
                self.nodes[i].args = args
                self.start_node(i)

            # Wait for RPC connections to be ready
            for node in self.nodes:
                node.wait_for_rpc_connection()

            # Create a 199-block-long chain; each of the 4 first nodes
            # gets 25 mature blocks and 25 immature.
            # The 4th node gets only 24 immature blocks so that the very last
            # block in the cache does not age too much (have an old tip age).
            # This is needed so that we are out of IBD when the test starts,
            # see the tip age check in IsInitialBlockDownload().
            #
            # We need to initialize also nodes' wallets with some genesis funds
            # and we use the last 4 addresses in the genesis to do so.

            for peer in range(4):
                self.nodes[peer].importmasterkey(regtest_mnemonics[-(peer+1)]['mnemonics'])
                self.nodes[peer].importprivkey(self.nodes[peer].get_deterministic_priv_key()[1])

            for i in range(8):
                self.nodes[0].generatetoaddress(25 if i != 7 else 24, self.nodes[i % 4].get_deterministic_priv_key().address)
            sync_blocks(self.nodes)

            for n in self.nodes:
                assert_equal(n.getblockchaininfo()["blocks"], 199)

            # Shut them down, and clean up cache directories:
            self.stop_nodes()
            self.nodes = []

            def cache_path(n, *paths):
                return os.path.join(get_datadir_path(self.options.cachedir, n), "regtest", *paths)

            for i in range(MAX_NODES):
                # UNIT-E TODO [0.18.0]: Which one should we use?
                os.rmdir(cache_path(i, 'wallets'))  # Remove empty wallets dir
                # shutil.rmtree(cache_path(i, 'wallets'))  # Remove cache generators' wallets dir
                for entry in os.listdir(cache_path(i)):
                    if entry not in ['chainstate', 'blocks', 'snapshots', 'finalization', 'votes']:
                        os.remove(cache_path(i, entry))

        for i in range(self.num_nodes):
            from_dir = get_datadir_path(self.options.cachedir, i)
            to_dir = get_datadir_path(self.options.tmpdir, i)
            shutil.copytree(from_dir, to_dir)
            initialize_datadir(self.options.tmpdir, i)  # Overwrite port/rpcport in unit-e.conf

    def _initialize_chain_clean(self):
        """Initialize empty blockchain for use by the test.

        Create an empty blockchain and num_nodes wallets.
        Useful if a test case wants complete control over initialization."""
        for i in range(self.num_nodes):
            initialize_datadir(self.options.tmpdir, i)

    def skip_if_no_py3_zmq(self):
        """Attempt to import the zmq package and skip the test if the import fails."""
        try:
            import zmq  # noqa
        except ImportError:
            raise SkipTest("python3-zmq module not available.")

    def skip_if_no_unit_e_zmq(self):
        """Skip the running test if unit-e has not been compiled with zmq support."""
        if not self.is_zmq_compiled():
            raise SkipTest("unit-e has not been built with zmq enabled.")

    def skip_if_no_wallet(self):
        """Skip the running test if wallet has not been compiled."""
        if not self.is_wallet_compiled():
            raise SkipTest("wallet has not been compiled.")

    def skip_if_no_cli(self):
        """Skip the running test if unit-e-cli has not been compiled."""
        if not self.is_cli_compiled():
            raise SkipTest("unit-e-cli has not been compiled.")

    def is_cli_compiled(self):
        """Checks whether unit-e-cli was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))

        # UNIT-E TODO [0.18.0]: They just got back to the version from 2014 year:
        # https://github.com/bitcoin/bitcoin/blob/0.18/test/functional/test_framework/test_framework.py#L559
        # return config["components"].getboolean("ENABLE_UTILS")
        return config["components"].getboolean("ENABLE_CLI")

    def is_wallet_compiled(self):
        """Checks whether the wallet module was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))

        return config["components"].getboolean("ENABLE_WALLET")

    def is_zmq_compiled(self):
        """Checks whether the zmq module was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))

        return config["components"].getboolean("ENABLE_ZMQ")

    def is_usbdevice_compiled(self):
        """Checks whether the zmq module was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile))

        return config["components"].getboolean("ENABLE_USBDEVICE")
