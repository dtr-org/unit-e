FreeBSD build guide
======================
(updated for FreeBSD 12.0)

This guide describes how to build unit-e and command-line utilities on FreeBSD.

This guide does not contain instructions for building the GUI.

## Preparation

You will need the following dependencies, which can be installed as root via pkg:

```shell
pkg install autoconf automake boost-libs git gmake libevent libtool openssl pkgconf

git clone https://github.com/dtr-org/unit-e.git
```

In order to run the test suite (recommended), you will need to have Python 3 installed:

```shell
pkg install python3
```

See [dependencies.md](dependencies.md) for a complete overview.

### Building BerkeleyDB

BerkeleyDB is only necessary for the wallet functionality. To skip this, pass
`--disable-wallet` to `./configure` and skip to the next section.

```shell
./contrib/install_db4.sh `pwd`
export BDB_PREFIX="$PWD/db4"
```

## Building unit-e

**Important**: Use `gmake` (the non-GNU `make` will exit with an error):

```shell
./autogen.sh

./configure                  # to build with wallet OR
./configure --disable-wallet # to build without wallet

gmake       # use -jX here for parallelism
gmake check # Run tests if Python 3 is available
```
