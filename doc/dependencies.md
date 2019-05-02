Dependencies
============

These are the dependencies currently used by unit-e. You can find instructions for installing them in the `build-*.md` file for your platform.

| Dependency | Version used | Minimum required | CVEs | Shared | [Bundled Qt library](https://doc.qt.io/qt-5/configure-options.html) |
| --- | --- | --- | --- | --- | --- |
| Berkeley DB | [4.8.30](http://www.oracle.com/technetwork/database/database-technologies/berkeleydb/downloads/index.html) | 4.8.x | No |  |  |
| Boost | [1.64.0](https://www.boost.org/users/download/) | [1.47.0](https://github.com/unite/unite/pull/8920) | No |  |  |
| Clang |  | [3.3+](https://llvm.org/releases/download.html) (C++11 support) |  |  |  |
| D-Bus | [1.10.18](https://cgit.freedesktop.org/dbus/dbus/tree/NEWS?h=dbus-1.10) |  | No | Yes |  |
| Expat | [2.2.5](https://libexpat.github.io/) |  | No | Yes |  |
| GCC |  | [4.8+](https://gcc.gnu.org/) (C++11 support) |  |  |  |
| HarfBuzz-NG |  |  |  |  |  |
| hidapi | [0.8.0-rc1](https://github.com/signal11/hidapi/releases/tag/hidapi-0.8.0-rc1) |  | No | Yes |
| libevent | [2.1.8-stable](https://github.com/libevent/libevent/releases) | 2.0.22 | No |  |  |
| MiniUPnPc | [2.0.20180203](http://miniupnp.free.fr/files) |  | No |  |  |
| OpenSSL | [1.0.1k](https://www.openssl.org/source) |  | Yes |  |  |
| PCRE |  |  |  |  | [Yes](https://github.com/unite/unite/blob/master/depends/packages/qt.mk#L66) |
| Python (tests) |  | [3.4](https://www.python.org/downloads) |  |  |  |
| ZeroMQ | [4.3.1](https://github.com/zeromq/libzmq/releases) |  | No |  |  |
| zlib | [1.2.11](https://zlib.net/) |  |  |  | No |
