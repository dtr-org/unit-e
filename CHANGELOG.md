# unit-e changelog

## 0.1.0

Initial release marking the start of the alpha testnet. This is a source code
only release. Check it out from git and follow the instructions in the README.md
to build and run it.

The initial unit-e release is based on [Bitcoin Core
0.16.3](https://github.com/bitcoin/bitcoin/releases/tag/v0.16.3). It adds a
number of significant changes:

* Replace Proof of Work by Proof of Stake with on-chain block finalization and
  remote staking
* Native SegWit support
* Reduced bandwidth, storage, and time to sync of initial blockchain download by
  UTXO snapshots
* Enhanced privacy through Dandelion Lite
* Optimized block propagation through Graphene
* Canonical transaction ordering
* Hardware wallet support
* The GUI wallet is not part of the release
