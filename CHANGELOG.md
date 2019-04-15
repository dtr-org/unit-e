# unit-e changelog

## 0.1.0

Initial release marking the start of the alpha testnet. This is a source code
only release. Check it out from git and follow the instructions in the README.md
to build and run it.

The Release is based on [Bitcoin Core
0.16.3](https://github.com/bitcoin/bitcoin/releases/tag/v0.16.3). It adds a
number of significant changes:

* Replace Proof of Work by Esperanza Proof of Stake consensus protocol
* Staking wallet with remote staking support, no minimum stake required
* Finality, enabled by a new actor voting every epoch (50 blocks), with advanced
  on-chain lifecycle (deposit, vote, slash, withdraw). Security is maintained
  through financial incentives, including availability requirement and slashing
  for misbehavior
* Malleability protection through native SegWit support
* Reduced bandwidth, storage, and time to sync of initial blockchain download by
  UTXO snapshots
* Enhanced privacy through Dandelion Lite
* Optimized block propagation through Graphene
* Canonical transaction ordering
* Hardware wallet support
* Qt based GUI wallet has been removed. An alternative will be released later in
  a separate code repository.
