<img src="unit-e-logo.png" align="right">

# Unit-e

[![Build Status](https://travis-ci.com/dtr-org/unit-e.svg?token=bm5dxUvwqj2MkNmT6JSA&branch=master)](https://travis-ci.com/dtr-org/unit-e)
[![pullreminders](https://pullreminders.com/badge.svg)](https://pullreminders.com?ref=badge)

The unit-e client is the first implementation for the Unit-e cryptocurrency
network protocol, implemented in C++.

## What is Unit-e?

Unit-e is providing a scalable and decentralized monetary and payment network
based on current scientific research. It is the first project supported by the
[Distributed Technology Research Foundation (DTR)](https://dtr.org). Its design
is backed by the [research](https://dtr.org/research/) DTR is funding,
delivering the scalable performance needed to enter mainstream use. You can find
more information about the project on the [website](https://unit-e.io) and in
the [technical paper](https://unit-e.io/technical-design.pdf).

## The Unit-e client

:warning::warning::warning: WARNING: The client is under rapid development, is
subject to breaking protocol changes (consensus, blockchain, p2p, RPC) and
redoing from scratch of the alpha testnet.
Please check the [announcements page](https://docs.unit-e.io/announcements.html)
for information regarding such changes. :warning::warning::warning:

This repository hosts the implementation of the first Unit-e client: `unit-e`,
also known as the "Feuerland" client. It's based on the [Bitcoin C++
client](https://github.com/bitcoin/bitcoin) and introduces major improvements
and features such as:

* Replace Proof of Work (PoW) with Esperanza Proof of Stake (PoS).
  Unlike most blockchain projects, that's a complete rewrite
  of the consensus, leaving no trace of PoW while keeping the UTXO model and
  other areas (blockchain, p2p, wallet) functioning, potentially
  benefiting from future upstream improvements. In order to make it happen, we
  decoupled the layers and features to [components with dependency
  injection](https://github.com/dtr-org/unit-e/pull/137) following software
  design best practices. Allowing good testability and being able to do future
  modifications with confidence, including changes in the complex consensus
  layer
* Finality
  is enabled by finalizer nodes, voting every epoch (currently 50
  blocks), with advanced on-chain lifecycle (deposit, vote, slash, withdraw) on
  top of UTXO, using custom advanced scripts. Security is maintained through
  financial incentives, including availability requirement and slashing for
  misbehavior. Finality is essential for monetary applications, it mitigates
  against the major security issues of PoS (long-range, history revision,
  nothing-at-stake) and provides important scalability features such as pruning
  and fast-sync which otherwise wouldn't be possible in a fork-based PoS
  protocol
* Staking wallet with [remote staking](
  https://github.com/dtr-org/uips/blob/master/UIP-0015.md) support, activated
  by default, lightweight and without a minimum stake, allowing large
  participation rate and potential scale
* Enhanced [fork-choice rule](
  https://github.com/dtr-org/uips/blob/master/UIP-0012.md) by the
  best-finalized chain
* Malleability protection through [native SegWit support](
  https://github.com/dtr-org/uips/blob/master/UIP-0003.md)
* Reduced bandwidth, storage, and time to sync of initial blockchain download
  by [UTXO snapshots](https://github.com/dtr-org/uips/blob/master/UIP-0011.md)
* Enhanced privacy through [Dandelion Lite](
  https://github.com/dtr-org/unit-e/issues/210)
* [Canonical transactions ordering](
  https://github.com/dtr-org/uips/blob/master/UIP-0024.md),
  eliminating the need to send sorting metadata with each block,
  providing faster propagation and potential scability features as multi core
  validation
* Optimized block propagation through a hybrid set reconciliation protocol
  of [compact blocks](
  https://github.com/bitcoin/bips/blob/master/bip-0152.mediawiki)
  and [Graphene](https://github.com/dtr-org/uips/blob/master/UIP-0026.md)
* [Hardware wallet support](https://github.com/dtr-org/unit-e/issues/385),
  including remote staking

We regularly merge upstream changes into the unit-e code base and also strive to
contribute back changes which are relevant for upstream as we already have done.
The last upstream sync was with the [0.17 version](
https://github.com/bitcoin/bitcoin/tree/0.17), plus some changes cherry-picked
from later development branches.

## Alpha Testnet

With the [launch of the alpha testnet](
https://github.com/dtr-org/unit-e/milestone/11) we will start a regular cadence
of releases. The goals of opening the project and network are to further develop
the protocol, client and community:
* The protocol isn't complete and will be further developed with breaking
changes in order to reach our security & scalability goals (you can read about
it more in the [design paper](
https://unit-e.io/technical-design.pdf)).
* Areas such as crypto-economics and coin emission rate, need to be well
understood, fair and flexible - we're planning to use the testnet in order to
figure out important aspects such as the level of stake required to keep the
system secured and how should influence the emission rate.
* We're opening our code repository to the blockchain and open-source
community. We aspire to develop a community of active participants.

## Currency emission

The current emission rate is fixed over time and isn't meant to be definitive,
as we are still exploring the emission model from the economics & security
perspectives, which goes side by side with the consensus protocol that is going
to be further developed.

We plan to explore models with dynamic emission rate, where the network pays
for the security it needs. In PoS consensus, taking into account the number of
tokens being deposited/staked seems like the right direction.

As was also [researched](https://arxiv.org/pdf/1809.07468.pdf), time-based
emission, starting very high and decreasing over the years (in Bitcoin halving
every four years), isn't securing the protocol efficiently nor is it fair in
terms of compounding and the future currency distribution.

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

We strive to keep the unit-e codebase fully tested and covered by automated
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
* [Decision records and project-level information](
  https://github.com/dtr-org/unit-e-project)

## License

unit-e is released under the terms of the MIT license. See [COPYING](COPYING)
for more information or see https://opensource.org/licenses/MIT.
