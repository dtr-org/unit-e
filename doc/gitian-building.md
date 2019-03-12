Gitian building
================

### Setup your gitian host

If you are using Ubuntu, Debian or MacOS, running the `gitian-build.py` with
`--setup` flag should be enough. If you are having problems or if you are using
different operating system, you can find some more information in [the gitian
guide](https://github.com/bitcoin-core/docs/tree/master/gitian-building)
for Bitcoin Core on which the unit-e build process is based. Some of this
information might need adaption to unit-e, though.

### Build and sign unit-e for Linux, Windows, and OS X:

Run the automated script found in [contrib/gitian-build.py](/contrib/gitian-build.py) from your top level build directory:

	./unit-e/contrib/gitian-build.py [--setup] --build [--verify] [--os lwm] --signer <signer> --version <version>

Build output expected:

 - In the `unit-e-binaries/<version>` directory:
   * source tarball `unit-e-<version>.tar.gz`
   * linux 32- and 64-bit dist tarballs `unit-e-<version>-linux[32|64].tar.gz`
   * windows 32- and 64-bit unsigned installers `unit-e-<version>-win[32|64]-setup-unsigned.exe`
   * windows 32- and 64-bit unsigned dist zips `unit-e-<version>-win[32|64].zip`
   * OS X unsigned dist tarball `unit-e-<version>-osx64.tar.gz`

 - In the `unit-e-sigs` directory:
   * gitian build result `<version>-<linux|{win|osx}-unsigned>/<signer>/unit-e-<linux|win|osx>-<version>-build.assert`
   * build result signature `<version>-<linux|{win|osx}-unsigned>/<signer>/unit-e-<linux|win|osx>-<version>-build.assert.sig`

The changes will be automatically committed. Push your results to the signatures repository.

The Mac OS X builds require some files from the Apple SDK. See the notes at the
end of the file how to provide them.

### Optional: Seed the Gitian sources cache and offline git repositories

By default, Gitian will fetch source files as needed. To cache them ahead of time:

    pushd ./gitian-builder
    make -C ../unit-e/depends download SOURCES_PATH=`pwd`/cache/common
    popd

Only missing files will be fetched, so this is safe to re-run for each build.

NOTE: Offline builds must use the `--url` flag to ensure Gitian fetches only from local URLs. For example:

    ./unit-e/contrib/gitian-build.py --url file:///path/to/unit-e {rest of arguments}

It is not possible to specify the path to local signatures repository.

### Verify other gitian builders signatures to your own.

Add other unit-e Gitian builders' keys to your gpg keyring, and/or refresh keys: See `../unit-e/contrib/gitian-keys/README.md`.

Run the build script with `--verify` flag. Note that some of the signatures might be missing at this point, so pay attention to the script output.

### Create Windows detached signatures (Codesigner only)

This is only to be done by the codesigner.

- Only one person handles codesigning. Everyone else should skip to the next step.
- Only once the Windows builds each have 3 matching signatures may they be signed with their respective release keys.

	./unit-e/contrib/gitian-build.py [--setup] [--build] --codesign -o w --signer <signer> --version <version> [--win-code-cert unit-e/contrib/windeploy/win-codesign.cert --win-code-key <path to corresponding key>]

The signatures will be commited in `unit-e-sigs/<version>-detached`. Push them to the repository.

### Create signed Windows binaries

Wait for Windows detached signatures:
- Once the Windows builds each have 3 matching signatures, they will be signed with their respective release keys.
- Detached signatures will then be committed to the [unit-e-sigs](https://github.com/dtr-org/unit-e-sigs) repository, which can be combined with the unsigned apps to create signed binaries.

Create the signed Windows binary

	./unit-e/contrib/gitian-build.py [--setup] [--build] --sign [--verify] [--os w] --signer <signer> --version <version>

The signatures for signed builds will be committed in `unit-e-sigs/<version>-win-signed/<signer>`. Verify the signed binaries and push the signatures to the repository.

### Notes for Mac Gitian builds

Apple's version of binutils (called cctools) contains lots of functionality
missing in the FSF's binutils. In addition to extra linker options for
frameworks and sysroots, several other tools are needed as well such as
install_name_tool, lipo, and nmedit. These do not build under linux, so they
have been patched to do so. The work here was used as a starting point:
[mingwandroid/toolchain4](https://github.com/mingwandroid/toolchain4).

In order to build a working toolchain, the following source packages are needed
from Apple: cctools, dyld, and ld64.

These tools inject timestamps by default, which produce non-deterministic
binaries. The ZERO_AR_DATE environment variable is used to disable that.

This version of cctools has been patched to use the current version of clang's
headers and its libLTO.so rather than those from llvmgcc, as it was
originally done in toolchain4.

To complicate things further, all builds must target an Apple SDK. These SDKs
are free to download, but not redistributable.
To obtain it, register for a developer account, then download the [Xcode 7.3.1 dmg](https://developer.apple.com/devcenter/download.action?path=/Developer_Tools/Xcode_7.3.1/Xcode_7.3.1.dmg).

This file is several gigabytes in size, but only a single directory inside is
needed:
```
Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk
```

Unfortunately, the usual linux tools (7zip, hpmount, loopback mount) are incapable of opening this file.
To create a tarball suitable for Gitian input, there are two options:

Using Mac OS X, you can mount the dmg, and then create it with:
```
  $ hdiutil attach Xcode_7.3.1.dmg
  $ tar -C /Volumes/Xcode/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/ -czf MacOSX10.11.sdk.tar.gz MacOSX10.11.sdk
```

Alternatively, you can use 7zip and SleuthKit to extract the files one by one.
The script contrib/macdeploy/extract-osx-sdk.sh automates this. First ensure
the dmg file is in the current directory, and then run the script. You may wish
to delete the intermediate 5.hfs file and MacOSX10.11.sdk (the directory) when
you've confirmed the extraction succeeded.

```bash
apt-get install p7zip-full sleuthkit
contrib/macdeploy/extract-osx-sdk.sh
rm -rf 5.hfs MacOSX10.11.sdk
```

The Gitian descriptors build 2 sets of files: Linux tools, then Apple binaries
which are created using these tools. The build process has been designed to
avoid including the SDK's files in Gitian's outputs. All interim tarballs are
fully deterministic and may be freely redistributed.

The MacOSX10.11.sdk.tar.gz file needs to be copied to the gitian-build input
directory.
