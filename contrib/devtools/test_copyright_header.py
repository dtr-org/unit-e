# Copyright (c) 2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Unit tests for copyright_header.py
#
# Run them with `python3 -m unittest -v test_copyright_header`

import unittest
from unittest import mock

import tempfile
import os
from pathlib import Path
from copyright_header import exec_insert_header


class TestCopyrightHeader(unittest.TestCase):
    def run_and_test_insertion(self, original, expected_result, header_type):
        try:
            if header_type == "python":
                suffix = ".py"
            elif header_type == "javascript":
                suffix = ".js"
            else:
                suffix = ".cpp"
            with tempfile.NamedTemporaryFile(delete=False, suffix=suffix) as file:
                file.write(original.encode('utf-8'))

            with mock.patch('copyright_header.get_git_change_year_range', return_value=[2017,2018]):
                exec_insert_header(file.name, header_type)

            with open(file.name) as result_file:
                result = result_file.read()

            self.assertEqual(result, expected_result)
        finally:
            os.remove(file.name)

    def test_python(self):
        original = '''import something

def main():
    do_nothing()
'''
        expected_result = '''# Copyright (c) 2017-2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import something

def main():
    do_nothing()
'''
        self.run_and_test_insertion(original, expected_result, 'python')

        # Should not insert the header twice
        with self.assertRaises(SystemExit):
            self.run_and_test_insertion(expected_result, expected_result, 'python')

    def test_python_with_shebang(self):
        original = '''#!/usr/bin/env python3
import something

def main():
    do_nothing()
'''
        expected_result = '''#!/usr/bin/env python3
# Copyright (c) 2017-2018 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import something

def main():
    do_nothing()
'''
        self.run_and_test_insertion(original, expected_result, 'python')

        # Should not insert the header twice
        with self.assertRaises(SystemExit):
            self.run_and_test_insertion(expected_result, expected_result, 'python')

    def test_cpp(self):
        original = '''#include "something.h"

void main() {
    do_nothing();
}
'''
        expected_result = '''// Copyright (c) 2017-2018 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "something.h"

void main() {
    do_nothing();
}
'''
        self.run_and_test_insertion(original, expected_result, 'cpp')

        # Should not insert the header twice
        with self.assertRaises(SystemExit):
            self.run_and_test_insertion(expected_result, expected_result, 'cpp')

    def test_javascript(self):
        original = '''const something = require('something');

do_nothing();
'''
        expected_result = '''/*
 * Copyright (C) 2017-2018 The Unit-e developers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

const something = require('something');

do_nothing();
'''
        self.run_and_test_insertion(original, expected_result, 'javascript')

        # Should not insert the header twice
        with self.assertRaises(SystemExit):
            self.run_and_test_insertion(expected_result, expected_result, 'javascript')


if __name__ == '__main__':
    unittest.main()
