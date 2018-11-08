# Maintainers

UnitE is an open, community-based project. It's open for
[contributions](CONTRIBUTING.md).

There is a team of maintainers who have write access to the repository, are
reviewing patches, merging pull requests, acting as points of contacts, and
helping to get questions and other requests to the project answered.

The maintainer team can be reached on GitHub as @unit-e/maintainers, it consists
of the following individuals (listed in alphabetical order of the GitHub
handle):

* [Aleksandr Mikhailov (@AM5800)](https://github.com/AM5800)
* [Andrés Correa Casablanca (@castarco)](https://github.com/castarco)
* [Mihai Ciumeica (@cmihai)](https://github.com/cmihai)
* [Cornelius Schumacher (@cornelius)](https://github.com/cornelius)
* [Stanislav Frolov (@frolosofsky)](https://github.com/frolosofsky)
* [Matteo Sumberaz (@Gnappuraz)](https://github.com/Gnappuraz)
* [Kostiantyn Stepaniuk (@kostyantyn)](https://github.com/kostyantyn)
* [Azat Nizametdinov (@Nizametdinov)](https://github.com/Nizametdinov)
* [Mateusz Morusiewicz (@Ruteri)](https://github.com/Ruteri)
* [Julian Fleischer (@scravy)](https://github.com/scravy)
* [Gil Danziger (@thothd)](https://github.com/thothd)

There are some areas of the code where changes might have a critical impact.
While the maintainers act as a team, these code areas need special review by
domain experts. These code areas and who are the experts is listed below. Add
them as reviewers on pull requests which are touching the respective areas.

We do assume a model of shared ownership of the code. So other maintainers, peer
reviewers, and contributors are encouraged to review and work on all parts of
the code.

The consensus implementation is at the core of UnitE. Changes to it can affect
the whole network so they need special review. The implementation consist of the
code to run nodes in the different required roles as proposers, validators, or
general nodes.

* Block proposal, the process of creating new blocks for the blockchain (mostly
  in `src/proposer`): [Julian
  Fleischer (@scravy)](https://github.com/scravy)
* Validator role, which mainly contains finalization, the process of making
  blocks non reversible through Byzantine agreement (mostly in `src/esperanza`): [Matteo
  Sumberaz (@Gnappuraz)](https://github.com/Gnappuraz)
* Shared validation logic run by all nodes (mostly in `src/staking`):  [Julian
  Fleischer (@scravy)](https://github.com/scravy),
  [Matteo Sumberaz (@Gnappuraz)](https://github.com/Gnappuraz)
* Permissioning system, manages validator whitelist and stores it in the
  blockchain, used for bootstrapping the blockchain (mostly in `src/esperanza/admin*`): [Aleksandr
  Mikhailov (@AM5800)](https://github.com/AM5800)
* Snapshots, fast syncing of clients through snapshots of the UTXO database
  (mostly in `src/snapshot/`):
  [Kostiantyn Stepaniuk (@kostyantyn)](https://github.com/kostyantyn)
