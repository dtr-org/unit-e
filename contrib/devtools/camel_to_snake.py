#!/usr/bin/env python3
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Replaces CamelCase with snake_case in symbols like m_helloWorld -> m_hello_world.

One-liner:
for f in $(find src/esperanza -name '*cpp' -or -name '*h'); do contrib/devtools/camel_to_snake.py $f; done
"""

import sys

def kill_camels(line):
    """
    Replaces words in the string from m_prefixHelloWorld style to m_prefix_hello_world one.
    Applies for any words.
    """
    result = ''
    # finite state machine or alike
    have_m = False
    have_delimiter = True
    have_full_prefix = False
    for idx, c in enumerate(line):
        if have_full_prefix and (c.isalpha() or c.isdigit() or c == '_'):
            if c.isupper():
                if result[-1:] != '_' and not line[idx-1].isupper():
                    result += '_'
                result += c.lower()
            else:
                result += c
        else:
            have_full_prefix = False
            if have_m and c == '_':
                have_full_prefix = True
                result += c
            elif have_delimiter and c == 'm':
                have_m = True
                result += c
            elif not(c.isalpha() or c.isdigit()):
                have_delimiter = True
                result += c
            else:
                result += c
                have_delimiter = False
                have_m = False
    return result

def main(argv):
    if (len(argv) < 2):
        print('Usage: {0} <filename>\n'
              '\n'
              'Replaces m_variableNameWhatever with m_variable_name_whatever in the specified <filename>')
        exit(1)
    filename = argv[1]
    result = []
    with open(filename, 'r', encoding='utf8') as f:
        for line in f:
            result += [kill_camels(line)]
    with open(filename, 'w', encoding='utf8') as f:
        for line in result:
            f.write(line)

if __name__ == '__main__':
    main(sys.argv)
