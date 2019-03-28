NetBSD build guide
======================
(updated for NetBSD 8.0)

This guide describes how to build united and command-line utilities on NetBSD.

Preparation
-------------

You will need the following modules, which can be installed via pkgsrc or pkgin:

```
autoconf
automake
boost
git
gmake
libevent
libtool
pkg-config
python37

Download the source code:
```
git clone https://github.com/dtr-org/unit-e
```

See [dependencies.md](dependencies.md) for a complete overview.

### Building BerkeleyDB

BerkeleyDB is only necessary for the wallet functionality. To skip this, pass
`--disable-wallet` to `./configure` and skip to the next section.

It is recommended to use Berkeley DB 4.8. You cannot use the BerkeleyDB library
from ports, for the same reason as boost above (g++/libstd++ incompatibility).
If you have to build it yourself, you can use [the installation script included
in contrib/](/contrib/install_db4.sh) like so:

```shell
./contrib/install_db4.sh `pwd`
```

from the root of the repository. Then set `BDB_PREFIX` for the next section:

```shell
export BDB_PREFIX="$PWD/db4"
```

### Building unit-e

**Important**: Use `gmake` (the non-GNU `make` will exit with an error).

With wallet:
```
./autogen.sh
./configure CPPFLAGS="-I/usr/pkg/include" \
    LDFLAGS="-L/usr/pkg/lib" \
    BOOST_CPPFLAGS="-I/usr/pkg/include" \
    BOOST_LDFLAGS="-L/usr/pkg/lib" \
    BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" \
    BDB_CFLAGS="-I${BDB_PREFIX}/include"
```

Without wallet:
```
./autogen.sh
./configure --disable-wallet \
    CPPFLAGS="-I/usr/pkg/include" \
    LDFLAGS="-L/usr/pkg/lib" \
    BOOST_CPPFLAGS="-I/usr/pkg/include" \
    BOOST_LDFLAGS="-L/usr/pkg/lib"
```

Build and run the tests:
```bash
gmake # use -jX here for parallelism
gmake check
```
