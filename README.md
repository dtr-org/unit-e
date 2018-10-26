UnitE Core integration/staging tree
=====================================

[![Build Status](https://travis-ci.org/unite/unite.svg?branch=master)](https://travis-ci.org/unite/unite)

https://bitcoincore.org

What is UnitE?
----------------

UnitE is an experimental digital currency that enables instant payments to
anyone, anywhere in the world. UnitE uses peer-to-peer technology to operate
with no central authority: managing transactions and issuing money are carried
out collectively by the network. UnitE Core is the name of open source
software which enables the use of this currency.

For more information, as well as an immediately useable, binary version of
the UnitE Core software, see https://unite.org/en/download, or read the
[original whitepaper](https://bitcoincore.org/unite.pdf).

The UnitE team is committed to fostering a welcoming and harassment-free environment. All participants are expected to adhere to our [code of conduct](CODE_OF_CONDUCT.md).

License
-------

UnitE Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/unite/unite/tags) are created
regularly to indicate new official, stable release versions of UnitE Core.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

The developer [mailing list](https://lists.linuxfoundation.org/mailman/listinfo/unite-dev)
should be used to discuss complicated or controversial changes before working
on a patch set.

Developer IRC can be found on Freenode at #unite-core-dev.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test on short notice. Please be patient and help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python, that are run automatically on the build server.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The Travis CI system makes sure that every pull request is built for Windows, Linux, and OS X, and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
[UnitE Core's Transifex page](https://www.transifex.com/projects/p/unite/).

Translations are periodically pulled from Transifex and merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

**Important**: We do not accept translation changes as GitHub pull requests because the next
pull from Transifex would automatically overwrite them again.

Translators should also subscribe to the [mailing list](https://groups.google.com/forum/#!forum/unite-translators).
