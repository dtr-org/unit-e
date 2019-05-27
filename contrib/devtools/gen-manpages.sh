#!/usr/bin/env bash

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

UNIT_E=${UNIT_E:-$BINDIR/unit-e}
UNIT_E_CLI=${UNIT_E_CLI:-$BINDIR/unit-e-cli}
UNIT_E_TX=${UNIT_E_TX:-$BINDIR/unit-e-tx}
WALLET_TOOL=${WALLET_TOOL:-$BINDIR/unite-wallet}

[ ! -x $UNIT_E ] && echo "$UNIT_E not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
BTCVER=($($UNIT_E_CLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for unit-e if --version-string is not set,
# but has different outcomes for unit-e-cli.
echo "[COPYRIGHT]" > footer.h2m
$UNIT_E --version | sed -n '1!p' >> footer.h2m

for cmd in $UNIT_E $UNIT_E_CLI $UNIT_E_TX $WALLET_TOOL; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i'.orig' -e "s/\\\-${BTCVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
