# Maintainers

UnitE is an open, community-based project. It's open for
[contributions](CONTRIBUTING.md).

There is a team of maintainers who have write access to the repository, are
reviewing patches, merging pull requests, acting as points of contacts, and
helping to get questions and other requests to the project answered.

The maintainer team can be reached on GitHub as @unit-e/maintainers, it consists
of the following individuals (listed in alphabetical order of the GitHub
handle):

* [Aleksandr Mikhailov](https://github.com/AM5800)
  ([@AM5800](https://github.com/AM5800))
* [Cornelius Schumacher](https://github.com/cornelius)
  ([@cornelius](https://github.com/cornelius))
* [Stanislav Frolov](https://github.com/frolosofsky)
  ([@frolosofsky](https://github.com/frolosofsky))
* [Matteo Sumberaz](https://github.com/Gnappuraz)
  ([@Gnappuraz](https://github.com/Gnappuraz))
* [Kostiantyn Stepaniuk](https://github.com/kostyantyn)
  ([@kostyantyn](https://github.com/kostyantyn))
* [Azat Nizametdinov](https://github.com/Nizametdinov)
  ([@Nizametdinov](https://github.com/Nizametdinov))
* [Mateusz Morusiewicz](https://github.com/Ruteri)
  ([@Ruteri](https://github.com/Ruteri))
* [Julian Fleischer](https://github.com/scravy)
  ([@scravy](https://github.com/scravy))
* [Gil Danziger](https://github.com/thothd)
  ([@thothd](https://github.com/thothd))

There are some areas of the code where changes might have a critical impact.
While the maintainers act as a team, these code areas need special review by
domain experts. These code areas and who are the experts is listed below. Add
them as reviewers on pull requests which are touching the respective areas.

We do assume a model of shared ownership of the code. So other maintainers, peer
reviewers, and contributors are encouraged to review and work on all parts of
the code.

The Proof-of-Stake consensus protocol is at the core of UnitE. Changes to it
can affect the whole network so they need special review. The implementation
consist of the code to run nodes in the different required roles as proposers,
validators, or general nodes.

* Block proposal, the process of creating new blocks for the blockchain: [Julian
  Fleischer](https://github.com/scravy) ([@scravy](https://github.com/scravy))
* Validator role, which mainly contains finalization, the process of making
  blocks non reversible through Byzantine agreement: [Matteo
  Sumberaz](https://github.com/Gnappuraz)
  ([@Gnappuraz](https://github.com/Gnappuraz))
* Shared validation logic run by all nodes:  [Julian
  Fleischer](https://github.com/scravy) ([@scravy](https://github.com/scravy)),
  [Matteo Sumberaz](https://github.com/Gnappuraz)
  ([@Gnappuraz](https://github.com/Gnappuraz))
* Permissioning system, manages validator whitelist and stores it in the
  blockchain, used for bootstrapping the blockchain: [Aleksandr
  Mikhailov](https://github.com/AM5800) ([@AM5800](https://github.com/AM5800))
* Snapshots, fast syncing of clients through snapshots of the UTXO database:
  [Kostiantyn Stepaniuk](https://github.com/kostyantyn)
  ([@kostyantyn](https://github.com/kostyantyn))
