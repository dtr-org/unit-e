#!/usr/bin/env python3

"""
Usage: base58_fixup [-i] <file1> [<file2> ...]

Searches the files for strings containing base58-check- and bech32-encoded addresses
and replaces their prefixes based on the given map.
"""

import sys
import base58
import re
import codecs


base58_prefixes = {
    b'\x00': b'\x6F',
    b'\x05': b'\xC4',
    b'\x80': b'\xEF',
    b'\x04\x88\xad\xe4': b'\x04\x35\x83\x94',
    b'\x04\x88\xb2\x1e': b'\x04\x35\x87\xCF',
}

bech32_hrps = {
    'ue': 'tue',
    'bcrt': 'uert',
}

def fixup_base58_prefix(string):
    base58_old = string
    bytes_old = base58.b58decode_check(base58_old)

    for k, v in base58_prefixes.items():
        if bytes_old[0:len(k)] == k:
            bytes_new = v + bytes_old[len(k):]
            break
    else:
        print('Failed to decode %s' % string, file=sys.stderr)
        return string
    return codecs.decode(base58.b58encode_check(bytes_new), 'ascii')


def fixup_bech32_prefix(string):
    bech32_old = string
    hrp, data = bech32_decode(bech32_old)
    if not hrp and not data:
        raise ValueError()
    if hrp not in bech32_hrps:
        return string
    return bech32_encode(bech32_hrps[hrp], data)


def fixup_prefix(string):
    try:
        return fixup_base58_prefix(string)
    except ValueError:
        # Checksum error: not a base58 value
        pass
    try:
        return fixup_bech32_prefix(string)
    except ValueError:
        pass
    return None


def process_args():
    args = sys.argv[1:]
    in_place = ('-i' in args)
    infiles = [x for x in args if not x.startswith('-')]
    return infiles, in_place


def main():
    infiles, in_place = process_args()

    for infile in infiles:
        with open(infile, 'r') as f:
            text = f.read()

        replacements = 0
        for match in list(re.findall('\\b[%s]+\\b' % (str(base58.alphabet) + CHARSET), text)) + \
                     list(re.findall('\\b[%s]+\\b' % (str(base58.alphabet) + CHARSET), text)):
            replacement = fixup_prefix(match)
            if replacement:
                text = text.replace(match, replacement)
                replacements += 1

        if in_place:
            with open(infile, 'w') as f:
                f.write(text)
            print('Processes %s: made %d replacements.' % (infile, replacements), file=sys.stdout)
        else:
            sys.stdout.write(text)

"""Reference implementation for Bech32 and segwit addresses."""


CHARSET = "qpzry9x8gf2tvdw0s3jn54khce6mua7l"


def bech32_polymod(values):
    """Internal function that computes the Bech32 checksum."""
    generator = [0x3b6a57b2, 0x26508e6d, 0x1ea119fa, 0x3d4233dd, 0x2a1462b3]
    chk = 1
    for value in values:
        top = chk >> 25
        chk = (chk & 0x1ffffff) << 5 ^ value
        for i in range(5):
            chk ^= generator[i] if ((top >> i) & 1) else 0
    return chk


def bech32_hrp_expand(hrp):
    """Expand the HRP into values for checksum computation."""
    return [ord(x) >> 5 for x in hrp] + [0] + [ord(x) & 31 for x in hrp]


def bech32_verify_checksum(hrp, data):
    """Verify a checksum given HRP and converted data characters."""
    return bech32_polymod(bech32_hrp_expand(hrp) + data) == 1


def bech32_create_checksum(hrp, data):
    """Compute the checksum values given HRP and data."""
    values = bech32_hrp_expand(hrp) + data
    polymod = bech32_polymod(values + [0, 0, 0, 0, 0, 0]) ^ 1
    return [(polymod >> 5 * (5 - i)) & 31 for i in range(6)]


def bech32_encode(hrp, data):
    """Compute a Bech32 string given HRP and data values."""
    combined = data + bech32_create_checksum(hrp, data)
    return hrp + '1' + ''.join([CHARSET[d] for d in combined])


def bech32_decode(bech):
    """Validate a Bech32 string, and determine HRP and data."""
    if ((any(ord(x) < 33 or ord(x) > 126 for x in bech)) or
            (bech.lower() != bech and bech.upper() != bech)):
        return (None, None)
    bech = bech.lower()
    pos = bech.rfind('1')
    if pos < 1 or pos + 7 > len(bech) or len(bech) > 90:
        return (None, None)
    if not all(x in CHARSET for x in bech[pos+1:]):
        return (None, None)
    hrp = bech[:pos]
    data = [CHARSET.find(x) for x in bech[pos+1:]]
    if not bech32_verify_checksum(hrp, data):
        return (None, None)
    return (hrp, data[:-6])

if __name__ == '__main__':
    main()