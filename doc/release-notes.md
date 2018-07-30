UnitE Core version 0.16.2 is now available from:

  <https://bitcoincore.org/bin/unite-core-0.16.2/>

This is a new minor version release, with various bugfixes
as well as updated translations.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/unite/unite/issues>

To receive security and update notifications, please subscribe to:

  <https://bitcoincore.org/en/list/announcements/join/>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then run the
installer (on Windows) or just copy over `/Applications/UnitE-Qt` (on Mac)
or `united`/`unite-qt` (on Linux).

The first time you run version 0.15.0 or newer, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.

Note that the block database format also changed in version 0.8.0 and there is no
automatic upgrade code from before version 0.8 to version 0.15.0 or higher. Upgrading
directly from 0.7.x and earlier without re-downloading the blockchain is not supported.
However, as usual, old wallet versions are still supported.

Downgrading warning
-------------------

Wallets created in 0.16 and later are not compatible with versions prior to 0.16
and will not work if you try to use newly created wallets in older versions. Existing
wallets that were created with older versions are not affected by this.

Compatibility
==============

UnitE Core is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

UnitE Core should also work on most other Unix-like systems but is not
frequently tested on them.

0.16.2 change log
------------------

### Wallet
- #13622 `c04a4a5` Remove mapRequest tracking that just effects Qt display. (TheBlueMatt)
- #12905 `cfc6f74` [rpcwallet] Clamp walletpassphrase value at 100M seconds (sdaftuar)
- #13437 `ed82e71` wallet: Erase wtxOrderd wtx pointer on removeprunedfunds (MarcoFalke)

### RPC and other APIs
- #13451 `cbd2f70` rpc: expose CBlockIndex::nTx in getblock(header) (instagibbs)
- #13507 `f7401c8` RPC: Fix parameter count check for importpubkey (kristapsk)
- #13452 `6b9dc8c` rpc: have verifytxoutproof check the number of txns in proof structure (instagibbs)
- #12837 `bf1f150` rpc: fix type mistmatch in `listreceivedbyaddress` (joemphilips)
- #12743 `657dfc5` Fix csBestBlock/cvBlockChange waiting in rpc/mining (sipa)

### GUI
- #12432 `f78e7f6` [qt] send: Clear All also resets coin control options (Sjors)
- #12617 `21dd512` gui: Show messages as text not html (laanwj)
- #12793 `cf6feb7` qt: Avoid reseting on resetguisettigs=0 (MarcoFalke)

### Build system
- #13544 `9fd3e00` depends: Update Qt download url (fanquake)
- #12573 `88d1a64` Fix compilation when compiler do not support `__builtin_clz*` (532479301)

### Tests and QA
- #12447 `01f931b` Add missing signal.h header (laanwj)
- #12545 `1286f3e` Use wait_until to ensure ping goes out (Empact)
- #12804 `4bdb0ce` Fix intermittent rpc_net.py failure. (jnewbery)
- #12553 `0e98f96` Prefer wait_until over polling with time.sleep (Empact)
- #12486 `cfebd40` Round target fee to 8 decimals in assert_fee_amount (kallewoof)
- #12843 `df38b13` Test starting united with -h and -version (jnewbery)
- #12475 `41c29f6` Fix python TypeError in script.py (MarcoFalke)
- #12638 `0a76ed2` Cache only chain and wallet for regtest datadir (MarcoFalke)
- #12902 `7460945` Handle potential cookie race when starting node (sdaftuar)
- #12904 `6c26df0` Ensure united processes are cleaned up when tests end (sdaftuar)
- #13049 `9ea62a3` Backports (MarcoFalke)
- #13201 `b8aacd6` Handle disconnect_node race (sdaftuar)

### Miscellaneous
- #12887 `2291774` Add newlines to end of log messages (jnewbery)
- #12859 `18b0c69` Bugfix: Include <memory> for `std::unique_ptr` (luke-jr)
- #13131 `ce8aa54` Add Windows shutdown handler (ken2812221)
- #13652 `20461fc` rpc: Fix that CWallet::AbandonTransaction would leave the grandchildren, etc. active (Empact)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 532479301
- Ben Woosley
- Bernhard M. Wiedemann
- Chun Kuan Lee
- Cory Fields
- fanquake
- Gregory Sanders
- joemphilips
- John Newbery
- Kristaps Kaupe
- lmanners
- Luke Dashjr
- MarcoFalke
- Matt Corallo
- Pieter Wuille
- practicalswift
- Sjors Provoost
- Suhas Daftuar
- Wladimir J. van der Laan

And to those that reported security issues:

- Braydon Fuller
- Himanshu Mehta

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/unite/).
