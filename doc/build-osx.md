macOS Build Instructions and Notes
====================================
The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------
Install the macOS command line tools:

`xcode-select --install`

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

Dependencies
----------------------

    brew install automake berkeley-db4 libtool boost miniupnpc openssl pkg-config python libevent

See [dependencies.md](dependencies.md) for a complete overview.

Berkeley DB
-----------
It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [the installation script included in contrib/](/contrib/install_db4.sh)
like so

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

**Note**: You only need Berkeley DB if the wallet is enabled (see the section *Disable-Wallet mode* below).

Build unit-e
------------------------

1. Clone the unite source code and cd into `unite`

        git clone https://github.com/dtr-org/unit-e
        cd unit-e

2.  Build unit-e:

    Configure and build the unite binaries.

        ./autogen.sh
        ./configure
        make

3.  It is recommended to build and run the unit tests:

        make check

4.  You can also create a .dmg that contains the .app bundle (optional):

        make deploy

Running
-------

unit-e is now available at `./src/unit-e`

Before running, it's recommended that you create an RPC configuration file.

    echo -e "rpcuser=uniterpc\nrpcpassword=$(xxd -l 16 -p /dev/urandom)" > "/Users/${USER}/Library/Application Support/Unit-e/unit-e.conf"

    chmod 600 "/Users/${USER}/Library/Application Support/Unit-e/unit-e.conf"

The first time you run unit-e, it will start downloading the blockchain. This process could take several hours.

You can monitor the download process by looking at the debug.log file:

    tail -f $HOME/Library/Application\ Support/Unit-e/debug.log

Other commands:
-------

    ./src/unit-e -daemon # Starts the unite daemon.
    ./src/unit-e-cli --help # Outputs a list of command-line options.
    ./src/unit-e-cli help # Outputs a list of RPC commands when the daemon is running.

Using Qt Creator as IDE (untested)
------------------------
You can use Qt Creator as an IDE, for unite development.
Download and install the community edition of [Qt Creator](https://www.qt.io/download/).
Uncheck everything except Qt Creator during the installation process.

1. Make sure you installed everything through Homebrew mentioned above
2. Do a proper ./configure --enable-debug
3. In Qt Creator do "New Project" -> Import Project -> Import Existing Project
4. Enter "unit-e" as project name, enter src as location
5. Leave the file selection as it is
6. Confirm the "summary page"
7. In the "Projects" tab select "Manage Kits..."
8. Select the default "Desktop" kit and select "Clang (x86 64bit in /usr/bin)" as compiler
9. Select LLDB as debugger (you might need to set the path to your installation)
10. Start debugging with Qt Creator

Notes
-----

* Tested on OS X 10.10 Yosemite through macOS 10.13 High Sierra on 64-bit Intel processors only.
