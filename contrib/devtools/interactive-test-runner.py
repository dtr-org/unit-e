#!/usr/bin/env python3
# coding=utf-8
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Runs functional tests while reporting which ones are currently being run.

Tests to be run can be filtered by the arguments passed. Any test that is
run by the test runner has to match one of the arguments.

Example: Run all "wallet" and "rpc" tests.
    %(prog)s wallet_ rpc_

All options (--option) that do not target this test runner are passed
down to the scripts being invoked.
"""

import argparse
import os
import random
import subprocess
import sys
import tempfile
import time


def colorizer(mode):
    if os.name == 'posix':
        return lambda s: '\033[' + mode + 'm' + s + '\033[0m'
    else:
        return lambda s: s


bold = colorizer('1')
gray = colorizer('1;30')
red = colorizer('0;31')
yellow = colorizer('0;33')
blue = colorizer('0;34')

TICK, CROSS, CIRCLE = ('✓', '✖', '○') if sys.stdout.encoding == 'UTF-8' else (
'P', 'x', 'o')

TEST_EXIT_PASSED = 0
TEST_EXIT_SKIPPED = 77


class RunningJob:

    def __init__(self, test, log_stdout, log_stderr, proc):
        self.test = test
        self.log_stdout = log_stdout
        self.log_stderr = log_stderr
        self.proc = proc
        self.started = time.time()


class TestResult:

    def __init__(self, test, started, returncode, stdout, stderr):
        self.test = test
        self.started = started
        self.finished = time.time()
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr

    def format(self, test_name_length):
        if self.returncode == TEST_EXIT_SKIPPED:
            color = gray
            status = CIRCLE + ' Skipped'
        elif self.returncode == 0 and not self.stderr:
            color = blue
            status = TICK + ' Success'
        else:
            color = red
            status = CROSS + ' Failed'

        name = self.test.ljust(test_name_length)
        status = status.ljust(9)
        duration = self.finished - self.started

        return color, '%s | %s | %.0f s' % (name, status, duration)


class InteractiveTestRunner:

    def __init__(self, num_jobs, passon_args, tests_order):
        assert num_jobs >= 1

        self.repo_root = self.find_git_root()
        self.functional_test_root = os.path.join(self.repo_root, 'test',
                                                 'functional')
        self.tests = self.read_test_runner()
        self.num_jobs = num_jobs
        self.passon_args = passon_args
        self.line_length = 0
        self.tests_order = tests_order
        self.test_name_length = len(max(self.tests, key=len))

    @staticmethod
    def run_command(*args, **kwargs):
        stdout = subprocess.check_output(args, encoding='utf8', **kwargs)
        return stdout.strip()

    def find_git_root(self):
        return self.run_command('git', 'rev-parse', '--show-toplevel')

    def find_functional_tests(self, only_modified=False):
        args = 'git ls-files'.split()
        if only_modified:
            args.append('--modified')
        args.append('*.py')
        stdout = self.run_command(*args, cwd=self.functional_test_root)
        pyfiles = [line for line in stdout.split('\n') if '/' not in line]
        return pyfiles

    def read_test_runner(self):
        if self.functional_test_root not in sys.path:
            sys.path.append(self.functional_test_root)
        from test_runner import BASE_SCRIPTS
        return BASE_SCRIPTS

    def spawn_test(self, test):
        log_stdout = tempfile.SpooledTemporaryFile()
        log_stderr = tempfile.SpooledTemporaryFile()

        args = test.split()
        args[0] = os.path.join(self.functional_test_root, args[0])
        args.extend(self.passon_args)

        job = subprocess.Popen(
            args=args,
            universal_newlines=True,
            stdout=log_stdout,
            stderr=log_stderr
        )

        return RunningJob(test, log_stdout, log_stderr, job)

    def report_result_inline(self, test_result):
        color, line = test_result.format(self.test_name_length)
        missing = ' ' * max(0, self.line_length - len(line))
        self.line_length = 0
        print('\r%s%s' % (color(line), missing), flush=True)

    def report_running(self, running_jobs):
        def format_line(color=lambda s: s):
            jobs = ', '.join(map(
                lambda j: '%s (%d)' % (color(j.test), time.time() - j.started),
                running_jobs))
            return 'Running %s' % jobs

        line = format_line()
        self.line_length = max(len(line), self.line_length)
        print('\r%s' % format_line(yellow), end='', flush=True)

    def run(self, test_patterns, only_modified):
        relevant_files = self.find_functional_tests(only_modified=only_modified)

        def pattern_matches(name):
            return any(p in name for p in test_patterns)

        def is_relevant_file(name):
            return name.split()[0] in relevant_files

        remaining_tests = [name for name in self.tests if
                           pattern_matches(name) and is_relevant_file(name)]
        for order in self.tests_order:
            order(remaining_tests)
        running_jobs = []
        test_results = []

        def read(file):
            contents = file.read().decode('utf8')
            file.close()
            return contents

        while remaining_tests or running_jobs:
            for job in running_jobs:
                if job.proc.poll() is not None:
                    test_result = TestResult(
                        test=job.test,
                        started=job.started,
                        returncode=job.proc.returncode,
                        stdout=read(job.log_stdout),
                        stderr=read(job.log_stderr)
                    )
                    running_jobs.remove(job)
                    test_results.append(test_result)
                    self.report_result_inline(test_result)
            while remaining_tests and len(running_jobs) < self.num_jobs:
                test = remaining_tests.pop(0)
                running_jobs.append(self.spawn_test(test))
            self.report_running(running_jobs)
            time.sleep(0.5)


def partition(predicate, items):
    positives, negatives = [], []
    for item in items:
        (positives if predicate(item) else negatives).append(item)
    return positives, negatives


def main():
    parser = argparse.ArgumentParser(
        usage='%(prog)s [options] [script options] [which tests]',
        description=__doc__,
        formatter_class=argparse.RawTextHelpFormatter
    )

    parser.add_argument('--jobs', '-j', type=int, default=4,
                        help='how many test scripts to run in parallel. Default=4.')
    parser.add_argument('--shuffle', '-S', action='store_true',
                        help='shuffle the list of tests')
    parser.add_argument('--sort', '-s', action='store_true',
                        help='sort the list of tests alphabetically')
    parser.add_argument('--reverse', '-r', action='store_true',
                        help='reverse the list of tests (can be combined with --sort)')
    parser.add_argument('--modified', '-m', action='store_true',
                        help='only run tests that have been modified')

    args, unknown_args = parser.parse_known_args()
    passon_args, tests = partition(lambda arg: arg[:2] == '--', unknown_args)

    if not tests:
        tests = ['']

    tests_order = []
    if args.shuffle:
        tests_order.append(lambda ts: random.shuffle(ts))
    if args.sort:
        tests_order.append(lambda ts: ts.sort())
    if args.reverse:
        tests_order.append(lambda ts: ts.reverse())

    InteractiveTestRunner(
        num_jobs=args.jobs,
        passon_args=passon_args,
        tests_order=tests_order,
    ).run(test_patterns=tests, only_modified=args.modified)


if __name__ == '__main__':
    main()
