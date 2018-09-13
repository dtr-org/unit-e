#!/usr/bin/env bash

TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
SRCDIR=${SRCDIR:-$TOPDIR/src}
MANDIR=${MANDIR:-$TOPDIR/doc/man}

UNITED=${UNITED:-$SRCDIR/united}
UNITECLI=${UNITECLI:-$SRCDIR/unite-cli}
UNITETX=${UNITETX:-$SRCDIR/unite-tx}
UNITEQT=${UNITEQT:-$SRCDIR/qt/unite-qt}

[ ! -x $UNITED ] && echo "$UNITED not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
BTCVER=($($UNITECLI --version | head -n1 | awk -F'[ -]' '{ print $6, $7 }'))

# Create a footer file with copyright content.
# This gets autodetected fine for united if --version-string is not set,
# but has different outcomes for unite-qt and unite-cli.
echo "[COPYRIGHT]" > footer.h2m
$UNITED --version | sed -n '1!p' >> footer.h2m

for cmd in $UNITED $UNITECLI $UNITETX $UNITEQT; do
  cmdname="${cmd##*/}"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${MANDIR}/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${MANDIR}/${cmdname}.1
done

rm -f footer.h2m
