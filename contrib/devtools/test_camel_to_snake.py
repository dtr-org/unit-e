# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Unit tests for camel_to_snake.py
#
# Run them with `python3 -m unittest -v test_camel_to_snake`

import unittest

from camel_to_snake import kill_camels


class TestCamelToSnake(unittest.TestCase):
    def test_kill_camels(self):
        assert kill_camels('1 a m_a m_A m_aBcD') == '1 a m_a m_a m_a_bc_d'
        assert kill_camels('1+m_aBC\nm_aBc m_   m_aBCD') == '1+m_a_bc\nm_a_bc m_   m_a_bcd'
        assert kill_camels('m_Abc+m_aBc-maBC') == 'm_abc+m_a_bc-maBC'
        assert kill_camels('object.m_aBc.m_value = pointer->m_aBcDd') == 'object.m_a_bc.m_value = pointer->m_a_bc_dd'

    def test_keep_acronyms(self):
        assert kill_camels(' m_myUTXO') == ' m_my_utxo'


if __name__ == '__main__':
    unittest.main()
