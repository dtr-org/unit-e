#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

from enum import Enum
import logging
import json
import optparse
import os
import pdb
import shutil
import sys
import tempfile
import time

from .authproxy import JSONRPCException
from . import coverage
from .regtest_mnemonics import regtest_mnemonics
from .test_node import TestNode
from .util import (
    MAX_NODES,
    PortSeed,
    assert_equal,
    check_json_precision,
    connect_nodes_bi,
    disconnect_nodes,
    get_datadir_path,
    initialize_datadir,
    p2p_port,
    set_node_times,
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

# This parameter simulates the scenario that the node "never" reaches finalization.
# The purpose of it is to adapt Bitcoin tests to Unit-e which contradict with the finalization
# so the existing tests will perform the check within one dynasty.
# When this parameter is used, framework must be configured with `setup_clean_chain = True`
# to prevent using the cache which was generated with the finalization enabled.
DISABLE_FINALIZATION = '-esperanzaconfig={"epochLength": 99999}'

class UnitETestFramework():
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
        self.mocktime = 0
        self.supports_cli = False
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

        parser = optparse.OptionParser(usage="%prog [options]")
        parser.add_option("--nocleanup", dest="nocleanup", default=False, action="store_true",
                          help="Leave uniteds and test.* datadir on exit or error")
        parser.add_option("--noshutdown", dest="noshutdown", default=False, action="store_true",
                          help="Don't stop uniteds after the test execution")
        parser.add_option("--srcdir", dest="srcdir", default=os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../../../src"),
                          help="Source directory containing united/unite-cli (default: %default)")
        parser.add_option("--cachedir", dest="cachedir", default=os.path.normpath(os.path.dirname(os.path.realpath(__file__)) + "/../../cache"),
                          help="Directory for caching pregenerated datadirs")
        parser.add_option("--tmpdir", dest="tmpdir", help="Root directory for datadirs")
        parser.add_option("-l", "--loglevel", dest="loglevel", default="INFO",
                          help="log events at this level and higher to the console. Can be set to DEBUG, INFO, WARNING, ERROR or CRITICAL. Passing --loglevel DEBUG will output all logs to console. Note that logs at all levels are always written to the test_framework.log file in the temporary test directory.")
        parser.add_option("--tracerpc", dest="trace_rpc", default=False, action="store_true",
                          help="Print out all RPC calls as they are made")
        parser.add_option("--portseed", dest="port_seed", default=os.getpid(), type='int',
                          help="The seed to use for assigning port numbers (default: current process id)")
        parser.add_option("--coveragedir", dest="coveragedir",
                          help="Write tested RPC commands into this directory")
        parser.add_option("--configfile", dest="configfile",
                          help="Location of the test framework config file")
        parser.add_option("--pdbonfailure", dest="pdbonfailure", default=False, action="store_true",
                          help="Attach a python debugger if test fails")
        parser.add_option("--usecli", dest="usecli", default=False, action="store_true",
                          help="use unite-cli instead of RPC for all commands")
        self.add_options(parser)
        (self.options, self.args) = parser.parse_args()

        PortSeed.n = self.options.port_seed

        os.environ['PATH'] = self.options.srcdir + ":" + self.options.srcdir + "/qt:" + os.environ['PATH']

        check_json_precision()

        self.options.cachedir = os.path.abspath(self.options.cachedir)

        # Set up temp directory and start logging
        if self.options.tmpdir:
            self.options.tmpdir = os.path.abspath(self.options.tmpdir)
            os.makedirs(self.options.tmpdir, exist_ok=False)
        else:
            self.options.tmpdir = tempfile.mkdtemp(prefix="test")
        self._start_logging()

        success = TestStatus.FAILED

        try:
            if self.options.usecli and not self.supports_cli:
                raise SkipTest("--usecli specified but test does not support using CLI")
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

        if not self.options.noshutdown:
            self.log.info("Stopping nodes")
            if self.nodes:
                self.stop_nodes()
        else:
            for node in self.nodes:
                node.cleanup_on_exit = False
            self.log.info("Note: uniteds were not stopped and may still be running")

        if not self.options.nocleanup and not self.options.noshutdown and success != TestStatus.FAILED:
            self.log.info("Cleaning up")
            shutil.rmtree(self.options.tmpdir)
        else:
            self.log.warning("Not cleaning up dir %s" % self.options.tmpdir)

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
        sys.exit(exit_code)

    # Methods to override in subclass test scripts.
    def set_test_params(self):
        """Tests must this method to change default values for number of nodes, topology, etc"""
        raise NotImplementedError

    def add_options(self, parser):
        """Override this method to add command-line options to the test"""
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

    def setup_stake_coins(self, *args, rescan=True):
        for i, node in enumerate(args):
            node.importmasterkey(regtest_mnemonics[i+2]['mnemonics'], "", rescan)
            node.initial_stake = regtest_mnemonics[i+2]['balance']

    def run_test(self):
        """Tests must override this method to define test logic"""
        raise NotImplementedError

    # Public helper methods. These can be accessed by the subclass test scripts.

    def add_nodes(self, num_nodes, extra_args=None, rpchost=None, timewait=None, binary=None):
        """Instantiate TestNode objects"""

        if extra_args is None:
            extra_args = [[]] * num_nodes
        if binary is None:
            binary = [None] * num_nodes
        assert_equal(len(extra_args), num_nodes)
        assert_equal(len(binary), num_nodes)
        for i in range(num_nodes):
            print("Starting node " + str(i) + " with args: " + ' '.join(str(e) for e in extra_args[i]))
            self.nodes.append(TestNode(i, self.options.tmpdir, extra_args[i], rpchost, timewait=timewait, binary=binary[i], stderr=None, mocktime=self.mocktime, coverage_dir=self.options.coveragedir, use_cli=self.options.usecli))

    def start_node(self, i, *args, **kwargs):
        """Start a united"""

        node = self.nodes[i]

        node.start(*args, **kwargs)
        node.wait_for_rpc_connection()

        if self.options.coveragedir is not None:
            coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def start_nodes(self, extra_args=None, *args, **kwargs):
        """Start multiple uniteds"""

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
        """Stop a united test node"""
        self.nodes[i].stop_node(expected_stderr, wait=wait)
        self.nodes[i].wait_until_stopped()

    def stop_nodes(self, wait=0):
        """Stop multiple united test nodes"""
        for node in self.nodes:
            # Issue RPC to stop nodes
            node.stop_node(wait=wait)

        for node in self.nodes:
            # Wait for nodes to stop
            node.wait_until_stopped()

    def restart_node(self, i, extra_args=None):
        """Stop and start a test node"""
        self.stop_node(i)
        self.start_node(i, extra_args)

    def assert_start_raises_init_error(self, i, extra_args=None, expected_msg=None, *args, **kwargs):
        with tempfile.SpooledTemporaryFile(max_size=2**16) as log_stderr:
            try:
                self.start_node(i, extra_args, stderr=log_stderr, *args, **kwargs)
                self.stop_node(i)
            except Exception as e:
                assert 'united exited' in str(e)  # node must have shutdown
                self.nodes[i].running = False
                self.nodes[i].process = None
                if expected_msg is not None:
                    log_stderr.seek(0)
                    stderr = log_stderr.read().decode('utf-8')
                    if expected_msg not in stderr:
                        raise AssertionError("Expected error \"" + expected_msg + "\" not found in:\n" + stderr)
            else:
                if expected_msg is None:
                    assert_msg = "united should have exited with an error"
                else:
                    assert_msg = "united should have exited with expected error " + expected_msg
                raise AssertionError(assert_msg)

    def wait_for_node_exit(self, i, timeout):
        self.nodes[i].process.wait(timeout)

    def wait_for_transaction(self, txid, timeout=150):
        timeout += time.perf_counter()

        presence = dict.fromkeys(range(len(self.nodes)))

        while time.perf_counter() < timeout:
            all_have = True
            for node in self.nodes:
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
        vote = FromHex(CTransaction(), node.getrawtransaction(new_txs[0]))
        assert_equal(vote.get_type(), TxType.VOTE.name)

    def generate_sync(self, generator_node, nblocks=1):
        """
        Generates nblocks on a given node. Performing full sync after each block
        """
        generated_blocks = []
        for _ in range(nblocks):
            block = generator_node.generate(1)[0]
            generated_blocks.append(block)
            sync_blocks(self.nodes)

            # VoteIfNeeded is called on a background thread.
            # By syncing we ensure that all votes will be in the mempools at the
            # end of this iteration
            for node in self.nodes:
                node.syncwithvalidationinterfacequeue()

            sync_mempools(self.nodes)

        return generated_blocks

    def enable_mocktime(self):
        """Enable mocktime for the script.

        mocktime may be needed for scripts that use the cached version of the
        blockchain.  If the cached version of the blockchain is used without
        mocktime then the mempools will not sync due to IBD.

        For backwared compatibility of the python scripts with previous
        versions of the cache, this helper function sets mocktime to Jan 1,
        2014 + (201 * 10 * 60)"""
        self.mocktime = 1388534400 + (201 * 10 * 60)

    def disable_mocktime(self):
        self.mocktime = 0

    # Private helper methods. These should not be accessed by the subclass test scripts.

    def _start_logging(self):
        # Add logger and logging handlers
        self.log = logging.getLogger('TestFramework')
        self.log.setLevel(logging.DEBUG)
        # Create file handler to log all messages
        fh = logging.FileHandler(self.options.tmpdir + '/test_framework.log')
        fh.setLevel(logging.DEBUG)
        # Create console handler to log messages to stderr. By default this logs only error messages, but can be configured with --loglevel.
        ch = logging.StreamHandler(sys.stdout)
        # User can provide log level as a number or string (eg DEBUG). loglevel was caught as a string, so try to convert it to an int
        ll = int(self.options.loglevel) if self.options.loglevel.isdigit() else self.options.loglevel.upper()
        ch.setLevel(ll)
        # Format logs the same as united's debug.log with microprecision (so log files can be concatenated and sorted)
        formatter = logging.Formatter(fmt='%(asctime)s.%(msecs)03d000 %(name)s (%(levelname)s): %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
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

        Create a cache of a 200-block-long chain (with wallet) for MAX_NODES
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

            # Create cache directories, run uniteds:
            for i in range(MAX_NODES):
                datadir = initialize_datadir(self.options.cachedir, i)
                args = [os.getenv("UNITED", "united"), "-server", "-keypool=1", "-datadir=" + datadir, "-discover=0"]
                if i > 0:
                    args.append("-connect=127.0.0.1:" + str(p2p_port(0)))
                self.nodes.append(TestNode(i, self.options.cachedir, extra_args=[], rpchost=None, timewait=None, binary=None, stderr=None, mocktime=self.mocktime, coverage_dir=None))
                self.nodes[i].args = args
                self.start_node(i)

            # Wait for RPC connections to be ready
            for node in self.nodes:
                node.wait_for_rpc_connection()

            # Create a 200-block-long chain; each of the 4 first nodes
            # gets 25 mature blocks and 25 immature.
            # Note: To preserve compatibility with older versions of
            # initialize_chain, only 4 nodes will generate coins.
            #
            # We need to initialize also nodes' wallets with some genesis funds
            # and we use the last 4 addresses in the genesis to do so.
            #
            # Blocks are created with timestamps 10 minutes apart
            # starting from 2010 minutes in the past

            for peer in range(4):
                self.nodes[peer].importmasterkey(regtest_mnemonics[-(peer+1)]['mnemonics'])

            self.enable_mocktime()
            block_time = self.mocktime - (201 * 10 * 60)
            for i in range(2):
                for peer in range(4):
                    for j in range(25):
                        set_node_times(self.nodes, block_time)
                        self.nodes[peer].generate(1)
                        block_time += 10 * 60
                    # Must sync before next peer starts generating blocks
                    sync_blocks(self.nodes)

            # Shut them down, and clean up cache directories:
            self.stop_nodes()
            self.nodes = []
            self.disable_mocktime()

            def cache_path(n, *paths):
                return os.path.join(get_datadir_path(self.options.cachedir, n), "regtest", *paths)

            for i in range(MAX_NODES):
                for entry in os.listdir(cache_path(i)):
                    if entry not in ['wallets', 'chainstate', 'blocks', 'snapshots']:
                        os.remove(cache_path(i, entry))

        for i in range(self.num_nodes):
            from_dir = get_datadir_path(self.options.cachedir, i)
            to_dir = get_datadir_path(self.options.tmpdir, i)
            shutil.copytree(from_dir, to_dir)
            initialize_datadir(self.options.tmpdir, i)  # Overwrite port/rpcport in unite.conf

    def _initialize_chain_clean(self):
        """Initialize empty blockchain for use by the test.

        Create an empty blockchain and num_nodes wallets.
        Useful if a test case wants complete control over initialization."""
        for i in range(self.num_nodes):
            initialize_datadir(self.options.tmpdir, i)

class ComparisonTestFramework(UnitETestFramework):
    """Test framework for doing p2p comparison testing

    Sets up some united binaries:
    - 1 binary: test binary
    - 2 binaries: 1 test binary, 1 ref binary
    - n>2 binaries: 1 test binary, n-1 ref binaries"""

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def add_options(self, parser):
        parser.add_option("--testbinary", dest="testbinary",
                          default=os.getenv("UNITED", "united"),
                          help="united binary to test")
        parser.add_option("--refbinary", dest="refbinary",
                          default=os.getenv("UNITED", "united"),
                          help="united binary to use for reference nodes (if any)")

    def setup_network(self):
        extra_args = [['-whitelist=127.0.0.1']] * self.num_nodes
        if hasattr(self, "extra_args"):
            extra_args = self.extra_args
        self.add_nodes(self.num_nodes, extra_args,
                       binary=[self.options.testbinary] +
                       [self.options.refbinary] * (self.num_nodes - 1))
        self.start_nodes()

class SkipTest(Exception):
    """This exception is raised to skip a test"""
    def __init__(self, message):
        self.message = message
