Gitian building
================

### Setup your gitian host

If you are using Ubuntu, Debian or MacOS, running the `gitian-build.py` with `--setup` flag should be enough. If you are having problems or if you are using different operating system, see [the gitian guide](https://github.com/dtr-org/unit-e-docs/blob/master/gitian-building.md).

### Build and sign Unit-e Core for Linux, Windows, and OS X:

Run the automated script found in [contrib/gitian-build.py](/contrib/gitian-build.py) from your top level build directory:

	./unit-e/contrib/gitian-build.py [--setup] --build [--verify] [--os lwm] --signer <signer> --version <version>

Build output expected:

 - In the `unit-e-binaries/<version>` directory:
   * source tarball `unite-<version>.tar.gz`
   * linux 32- and 64-bit dist tarballs `unite-<version>-linux[32|64].tar.gz`
   * windows 32- and 64-bit unsigned installers `unite-<version>-win[32|64]-setup-unsigned.exe`
   * windows 32- and 64-bit unsigned dist zips `unite-<version>-win[32|64].zip`
   * OS X unsigned installer `unite-<version>-osx-unsigned.dmg`
   * OS X unsigned dist tarball `unite-<version>-osx64.tar.gz`

 - In the `unit-e-sigs` directory:
   * gitian build result `<version>-<linux|{win|osx}-unsigned>/<signer>/unite-<linux|win|osx>-<version>-build.assert`
   * build result signature `<version>-<linux|{win|osx}-unsigned>/<signer>/unite-<linux|win|osx>-<version>-build.assert.sig`

The changes will be automatically commited. Push your results to the signatures repository.

### Optional: Seed the Gitian sources cache and offline git repositories

By default, Gitian will fetch source files as needed. To cache them ahead of time:

    pushd ./gitian-builder
    make -C ../unite/depends download SOURCES_PATH=`pwd`/cache/common
    popd

Only missing files will be fetched, so this is safe to re-run for each build.

NOTE: Offline builds must use the `--url` flag to ensure Gitian fetches only from local URLs. For example:

    ./unit-e/contrib/gitian-build.py --url file:///path/to/unite {rest of arguments}

It is not possible to specify the path to local signatures repository.

### Verify other gitian builders signatures to your own. (Optional)

Add other unit-e Gitian builders' keys to your gpg keyring, and/or refresh keys: See `../unit-e/contrib/gitian-keys/README.md`.

Run the build script with `--verify` flag. Note that some of the signatures might be missing at this point, so pay attention to the script output.

### Create Windows/OS X detached signatures (Codesigner only)

Setup for codesigning Mac OS is described in [doc/README_osx.md](/doc/README_osx.md)

This is only to be done by the codesigner. Codesigning OS X binary is only possible on a Mac OS.
- Only one person handles codesigning. Everyone else should skip to the next step.
- Only once the Windows/OS X builds each have 3 matching signatures may they be signed with their respective release keys.

	./unit-e/contrib/gitian-build.py [--setup] [--build] --codesign -o [w|m] --signer <signer> --version <version> [--win-code-cert unit-e/contrib/windeploy/win-codesign.cert --win-code-key <path to corresponding key>]

The signatures will be commited in `unit-e-sigs/<version>-detached`. Push them to the repository.

### Create signed Windows/OS X binaries

Wait for Windows/OS X detached signatures:
- Once the Windows/OS X builds each have 3 matching signatures, they will be signed with their respective release keys.
- Detached signatures will then be committed to the [unit-e-sigs](https://github.com/unite-core/unit-e-sigs) repository, which can be combined with the unsigned apps to create signed binaries.

Create the signed Windows/OS X binary

	./unit-e/contrib/gitian-build.py [--setup] [--build] --sign [--verify] [--os wm] --signer <signer> --version <version>

Verify the signed binaries by running the build script with `--verify` if you did not provide the flag on signing.

The signatures for signed builds will be commited in `unit-e-sigs/<version>-[win|osx]-signed/<signer>`. Push them to the repository.
