<img src="unit-e-logo.png" align="right">

# unit-e

[![Build Status](https://travis-ci.com/dtr-org/unit-e.svg?token=bm5dxUvwqj2MkNmT6JSA&branch=master)](https://travis-ci.com/dtr-org/unit-e)

The unit-e client is the first implementation of a client for the Unit-e
cryptocurrency network protocol.

## What is Unit-e?

[Unit-e](https://dtr.org/unit-e) is a new cryptocurrency, providing a scalable
and decentralized monetary and payment network based on current scientific
research. It is the first project supported by the [Distributed Technology
Research foundation (DTR)](https://dtr.org). Its design is backed by the
[research](https://dtr.org/research/) DTR is funding, delivering the scalable
performance needed to enter mainstream use.

## The unit-e client

This repository hosts the implementation of the first Unit-e client: `unit-e`,
also known as the "Feuerland" client. It is based on the [Bitcoin
Core](https://github.com/bitcoin/bitcoin) code base as upstream and adds
features such as:

* Replace Proof of Work by [Proof of Stake with on-chain block
  finalization](https://github.com/dtr-org/unit-e-docs/blob/master/specs/spec_v1.0.md)
  and [remote staking](https://github.com/dtr-org/uips/blob/master/UIP-0015.md).
* Native SegWit support
* Reduced bandwidth, storage, and time to sync of initial blockchain download by
  [UTXO snapshots](https://github.com/dtr-org/uips/blob/master/UIP-0011.md)
* Enhanced privacy through Dandelion Lite
* Optimized block propagation through Graphene
* [Canonical transaction
  ordering](https://github.com/dtr-org/uips/blob/master/UIP-0024.md)
* Hardware wallet support

We regularly merge upstream changes into the unit-e code base and also strive to
contribute back changes which are relevant for upstream. The last upstream sync
was done with the [0.16](https://github.com/bitcoin/bitcoin/tree/0.16) version,
plus some changes cherry-picked from later development branches.

The client is in a pre-testnet development phase. With the [launch of the
testnet](https://github.com/dtr-org/unit-e/milestone/11) we will start a regular
cadence of releases.

## Running from source

To run unit-e from sources you will need to check it out from this GitHub
repository, compile it, and launch the resulting binary. This currently is the
only supported way of running it. Detailed instructions for a variety of
platforms can be found in the
[docs](https://github.com/dtr-org/unit-e/tree/master/doc) directory.

## Development

Development takes place on the `master` branch. All code changes go through
peer-reviewed and tested pull requests. We aim for meeting high standards as an
open source project and a collaborative community project. The contribution
workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The Unit-e team is committed to fostering a welcoming and harassment-free
environment. All participants are expected to adhere to our [code of
conduct](CODE_OF_CONDUCT.md).

## Testing

We strive to keep the unit-e code base fully tested and covered by automated
tests.

Unit tests can be compiled and run with: `make check`. Further details on
running and extending unit tests can be found in
[src/test/README.md](src/test/README.md).

There are also [functional tests](test), including regression and integration
tests. They are written in Python and most of them are also run as part of
automated continuous integration. These tests can be run locally with
`test/functional/test_runner.py`.

Unit and functional tests are [run on
Travis](https://travis-ci.com/dtr-org/unit-e) as part of our continuous
integration system. This tests the master branch and all pull requests. It makes
sure that code is checked, built and tested for Windows, Linux, and OS X
automatically before it gets merged into master.

Any additional testing, automated or manual, is very welcome. If you encounter
any issues or run into bugs please report them as
[issues](https://github.com/dtr-org/unit-e/issues).

## Related repositories

* [Unit-e improvement proposals (UIPs)](https://github.com/dtr-org/uips)
* [Documentation](https://github.com/dtr-org/docs.unit-e.io)
* [Decision records and project-level
  information](https://github.com/dtr-org/unit-e-docs)

## License

unit-e is released under the terms of the MIT license. See [COPYING](COPYING)
for more information or see https://opensource.org/licenses/MIT.
