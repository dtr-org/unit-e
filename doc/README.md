# unit-e developer documentation

This directory collects documentation primarily targeted at developers working
on the unit-e client itself.

*Note: Most of this documentation has been inherited from upstream but not
everything has been tested extensively with unit-e yet. If you see any issues
please report them as GitHub issues or submit a pull request to fix them.*

## Building

The following are developer notes on how to build unit-e on your native
platform. They are not complete guides, but include notes on the necessary
libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [OS X Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)

### Reproducible builds

- [Gitian Building Guide](gitian-building.md)
- [Deterministic OS X Dmg Notes](README_osx.md)

## Development

The unit-e repo's [root README](/README.md) contains relevant information on the
development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Process](release-process.md)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

## Miscellaneous

- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

## License

Distributed under the [MIT software license](/COPYING).

This product includes software developed by [The Bitcoin Core
developers](https://github.com/bitcoin/bitcoin). This product includes software
developed by the OpenSSL Project for use in the [OpenSSL
Toolkit](https://www.openssl.org/). This product includes cryptographic software
written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP
software written by Thomas Bernard.

Attribution of assets can be found at
[contrib/debian/copyright](../contrib/debian/copyright).
