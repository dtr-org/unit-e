Release Process
====================

Before every release candidate:

* Update translations (ping wumpus on IRC) see [translation_process.md](https://github.com/unite/unite/blob/master/doc/translation_process.md#synchronising-translations).

* Update manpages, see [gen-manpages.sh](https://github.com/unite/unite/blob/master/contrib/devtools/README.md#gen-manpagessh).

Before every minor and major release:

* Update [bips.md](bips.md) to account for changes since the last release.
* Update version in `configure.ac` (don't forget to set `CLIENT_VERSION_IS_RELEASE` to `true`)
* Write release notes (see below)
* Update `src/chainparams.cpp` nMinimumChainWork with information from the getblockchaininfo rpc.
* Update `src/chainparams.cpp` defaultAssumeValid  with information from the getblockhash rpc.
  - The selected value must not be orphaned so it may be useful to set the value two blocks back from the tip.
  - Testnet should be set some tens of thousands back from the tip due to reorgs there.
  - This update should be reviewed with a reindex-chainstate with assumevalid=0 to catch any defect
     that causes rejection of blocks in the past history.

Before every major release:

* Update hardcoded [seeds](/contrib/seeds/README.md), see [this pull request](https://github.com/unite/unite/pull/7415) for an example.
* Update [`BLOCK_CHAIN_SIZE`](/src/qt/intro.cpp) to the current size plus some overhead.
* Update `src/chainparams.cpp` chainTxData with statistics about the transaction count and rate.
* Update version of `contrib/gitian-descriptors/*.yml`: usually one'd want to do this on master after branching off the release - but be sure to at least do it before a new major release

### UnitE maintainers/release engineers, suggestion for writing release notes

Write release notes. git shortlog helps a lot, for example:

    git shortlog --no-merges v(current version, e.g. 0.7.2)..v(new version, e.g. 0.8.0)

(or ping @wumpus on IRC, he has specific tooling to generate the list of merged pulls
and sort them into categories based on labels)

Generate list of authors:

    git log --format='%aN' "$*" | sort -ui | sed -e 's/^/- /'

Tag version (or release candidate) in git

    git tag -s v(new version, e.g. 0.8.0)

### Build and sign UnitE Core for Linux, Windows, and OS X:

Run the automated script found in [contrib/gitian-build.py](/contrib/gitian-build.py) from your top level build directory with "--build" (and "--setup" if you want to automatically install dependencies).

Build output expected:

  1. source tarball (`unite-${VERSION}.tar.gz`)
  2. linux 32-bit and 64-bit dist tarballs (`unite-${VERSION}-linux[32|64].tar.gz`)
  3. windows 32-bit and 64-bit unsigned installers and dist zips (`unite-${VERSION}-win[32|64]-setup-unsigned.exe`, `unite-${VERSION}-win[32|64].zip`)
  4. OS X unsigned installer and dist tarball (`unite-${VERSION}-osx-unsigned.dmg`, `unite-${VERSION}-osx64.tar.gz`)
  5. Gitian signatures (in `unit-e-sigs/${VERSION}-<linux|{win,osx}-unsigned>/(your Gitian key)/`)

The changes will be automatically commited. Push your signatures to the repository.

### Optional: Seed the Gitian sources cache and offline git repositories

By default, Gitian will fetch source files as needed. To cache them ahead of time:

    pushd ./gitian-builder
    make -C ../unite/depends download SOURCES_PATH=`pwd`/cache/common
    popd

Only missing files will be fetched, so this is safe to re-run for each build.

NOTE: Offline builds must use the --url flag to ensure Gitian fetches only from local URLs. For example:
    ./unit-e/contrib/gitian-build.py --url file:///path/to/unite {rest of arguments}

It is not possible to specify the path to local signatures repository.

### Verify other gitian builders signatures to your own. (Optional)

Add other unit-e Gitian builders' keys to your gpg keyring, and/or refresh keys: See `../unit-e/contrib/gitian-keys/README.md`.

Run the build script with "--verify" flag

### Create Windows/OS X detached signatures

This is only to be done by the codesigner.
NOTE: Codesigning OS X binary is only possible on a Mac OS.

- Only one person handles codesigning. Everyone else should skip to the next step.
- Only once the Windows/OS X builds each have 3 matching signatures may they be signed with their respective release keys.

	./unit-e/contrib/gitian-build.py [--setup] [--build] --codesign -o [w|m] --signer <signer> --version <version> [--win-code-cert unit-e/contrib/windeploy/win-codesign.cert --win-code-key <path to corresponding key>]

The signatures will be commited in `unit-e-sigs/<version>-detached`. Push them to the repository.

### Create signed Windows/OS X binaries

Wait for Windows/OS X detached signatures:
- Once the Windows/OS X builds each have 3 matching signatures, they will be signed with their respective release keys.
- Detached signatures will then be committed to the [unit-e-sigs](https://github.com/unite-core/unit-e-sigs) repository, which can be combined with the unsigned apps to create signed binaries.

Create the signed Windows/OS X binary

	./unit-e/contrib/gitian-build.py [--setup] [--build] --sign -o [w|m] --signer <signer> --version <version>

Verify the signed binaries by running the build script with "--verify".

The signatures for signed builds will be commited in `unit-e-sigs/<version>-[win|osx]-signed/<signer>`. Push them to the repository.

### After 3 or more people have gitian-built and their results match:

- Create `SHA256SUMS.asc` for the builds, and GPG-sign it:

```bash
sha256sum * > SHA256SUMS
```

The list of files should be:
```
unite-${VERSION}-aarch64-linux-gnu.tar.gz
unite-${VERSION}-arm-linux-gnueabihf.tar.gz
unite-${VERSION}-i686-pc-linux-gnu.tar.gz
unite-${VERSION}-x86_64-linux-gnu.tar.gz
unite-${VERSION}-osx64.tar.gz
unite-${VERSION}-osx.dmg
unite-${VERSION}.tar.gz
unite-${VERSION}-win32-setup.exe
unite-${VERSION}-win32.zip
unite-${VERSION}-win64-setup.exe
unite-${VERSION}-win64.zip
```
The `*-debug*` files generated by the gitian build contain debug symbols
for troubleshooting by developers. It is assumed that anyone that is interested
in debugging can run gitian to generate the files for themselves. To avoid
end-user confusion about which file to pick, as well as save storage
space *do not upload these to the unite.org server, nor put them in the torrent*.

- GPG-sign it, delete the unsigned file:
```
gpg --digest-algo sha256 --clearsign SHA256SUMS # outputs SHA256SUMS.asc
rm SHA256SUMS
```
(the digest algorithm is forced to sha256 to avoid confusion of the `Hash:` header that GPG adds with the SHA256 used for the files)
Note: check that SHA256SUMS itself doesn't end up in SHA256SUMS, which is a spurious/nonsensical entry.

- Upload zips and installers, as well as `SHA256SUMS.asc` from last step, to the unite.org server
  into `/var/www/bin/unite-core-${VERSION}`

- A `.torrent` will appear in the directory after a few minutes. Optionally help seed this torrent. To get the `magnet:` URI use:
```bash
transmission-show -m <torrent file>
```
Insert the magnet URI into the announcement sent to mailing lists. This permits
people without access to `unite.org` to download the binary distribution.
Also put it into the `optional_magnetlink:` slot in the YAML file for
unite.org (see below for unite.org update instructions).

- Update unite.org version

  - First, check to see if the UnitE.org maintainers have prepared a
    release: https://github.com/unite-dot-org/unite.org/labels/Releases

      - If they have, it will have previously failed their Travis CI
        checks because the final release files weren't uploaded.
        Trigger a Travis CI rebuild---if it passes, merge.

  - If they have not prepared a release, follow the UnitE.org release
    instructions: https://github.com/unite-dot-org/unite.org#release-notes

  - After the pull request is merged, the website will automatically show the newest version within 15 minutes, as well
    as update the OS download links. Ping @saivann/@harding (saivann/harding on Freenode) in case anything goes wrong

- Announce the release:

  - unite-dev and unite-core-dev mailing list

  - UnitE Core announcements list https://bitcoincore.org/en/list/announcements/join/

  - bitcoincore.org blog post

  - Update title of #unite on Freenode IRC

  - Optionally twitter, reddit /r/UnitE, ... but this will usually sort out itself

  - Notify BlueMatt so that he can start building [the PPAs](https://launchpad.net/~unite/+archive/ubuntu/unite)

  - Archive release notes for the new version to `doc/release-notes/` (branch `master` and branch of the release)

  - Create a [new GitHub release](https://github.com/unite/unite/releases/new) with a link to the archived release notes.

  - Celebrate
