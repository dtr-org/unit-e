# Copyright (c) 2013-2016 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

DIST_SUBDIRS = secp256k1 univalue

AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS) $(HARDENED_LDFLAGS)
AM_CXXFLAGS = $(HARDENED_CXXFLAGS) $(ERROR_CXXFLAGS)
AM_CPPFLAGS = $(HARDENED_CPPFLAGS)
EXTRA_LIBRARIES =

if EMBEDDED_UNIVALUE
LIBUNIVALUE = univalue/libunivalue.la

$(LIBUNIVALUE): $(wildcard univalue/lib/*) $(wildcard univalue/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)
else
LIBUNIVALUE = $(UNIVALUE_LIBS)
endif

UNITE_INCLUDES=-I$(builddir) $(BDB_CPPFLAGS) $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS)

UNITE_INCLUDES += -I$(srcdir)/secp256k1/include
UNITE_INCLUDES += $(UNIVALUE_CFLAGS)

LIBUNITE_SERVER=libunite_server.a
LIBUNITE_COMMON=libunite_common.a
LIBUNITE_CONSENSUS=libunite_consensus.a
LIBUNITE_UTIL=libunite_util.a
LIBUNITE_CRYPTO=crypto/libunite_crypto.a
LIBSECP256K1=secp256k1/libsecp256k1.la

if ENABLE_ZMQ
LIBUNITE_ZMQ=libunite_zmq.a
endif
if BUILD_UNITE_LIBS
LIBUNITECONSENSUS=libuniteconsensus.la
endif
if ENABLE_WALLET
LIBUNITE_WALLET=libunite_wallet.a
endif
if ENABLE_USBDEVICE
LIBUNITE_USBDEVICE=libunite_usbdevice.a
endif

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  $(LIBUNITE_CRYPTO) \
  $(LIBUNITE_UTIL) \
  $(LIBUNITE_COMMON) \
  $(LIBUNITE_CONSENSUS) \
  $(LIBUNITE_SERVER) \
  $(LIBUNITE_USBDEVICE) \
  $(LIBUNITE_WALLET) \
  $(LIBUNITE_ZMQ)

lib_LTLIBRARIES = $(LIBUNITECONSENSUS)

bin_PROGRAMS =
noinst_PROGRAMS =
TESTS =
BENCHMARKS =

if BUILD_UNIT_E
  bin_PROGRAMS += unit-e
endif

if BUILD_UNITE_UTILS
  bin_PROGRAMS += unit-e-cli unit-e-tx
endif

.PHONY: FORCE check-symbols check-security
# unit-e #
UNITE_CORE_H = \
  addrdb.h \
  addrman.h \
  base58.h \
  bech32.h \
  better-enums/enum.h \
  better-enums/enum_set.h \
  blockchain/blockchain_behavior.h \
  blockchain/blockchain_custom_parameters.h \
  blockchain/blockchain_genesis.h \
  blockchain/blockchain_interfaces.h \
  blockchain/blockchain_rpc.h \
  blockchain/blockchain_parameters.h \
  blockchain/blockchain_types.h \
  blockdb.h \
  bloom.h \
  blockencodings.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkqueue.h \
  clientversion.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/tx_verify.h \
  consensus/ltor.h \
  consensus/ltor.cpp \
  core_io.h \
  core_memusage.h \
  cuckoocache.h \
  dependency.h \
  dependency_injector.h \
  esperanza/admincommand.h \
  esperanza/adminparams.h \
  esperanza/adminstate.h \
  esperanza/checkpoint.h \
  esperanza/checks.h \
  esperanza/finalizationparams.h \
  esperanza/finalizationstate.h \
  esperanza/finalizationstate_data.h \
  esperanza/init.h \
  esperanza/validator.h \
  esperanza/validatorstate.h \
  esperanza/vote.h \
  esperanza/walletextension.h \
  esperanza/walletextension_deps.h \
  esperanza/walletstate.h \
  extkey.h \
  finalization/state_db.h \
  finalization/state_processor.h \
  finalization/state_repository.h \
  finalization/vote_recorder.h \
  fs.h \
  httprpc.h \
  httpserver.h \
  iblt.h \
  iblt_params.h \
  iblt_params.table \
  indirectmap.h \
  init.h \
  injector.h \
  injector_config.h \
  key.h \
  key/mnemonic/mnemonic.h \
  key/mnemonic/chinese_simplified.h \
  key/mnemonic/chinese_traditional.h \
  key/mnemonic/english.h \
  key/mnemonic/french.h \
  key/mnemonic/japanese.h \
  key/mnemonic/spanish.h \
  key/mnemonic/italian.h \
  key/mnemonic/korean.h \
  keystore.h \
  dbwrapper.h \
  limitedmap.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  net.h \
  net_processing.h \
  netaddress.h \
  netbase.h \
  netmessagemaker.h \
  noui.h \
  p2p/embargoman.h \
  p2p/embargoman_init.h \
  p2p/finalizer_commits_handler.h \
  p2p/finalizer_commits_handler_impl.h \
  p2p/finalizer_commits_types.h \
  p2p/graphene.h \
  p2p/graphene_common.h \
  p2p/graphene_hasher.h \
  p2p/graphene_messages.h \
  p2p/graphene_receiver.h \
  p2p/graphene_sender.h \
  policy/feerate.h \
  policy/fees.h \
  policy/policy.h \
  policy/rbf.h \
  proposer/block_builder.h \
  proposer/eligible_coin.h \
  proposer/multiwallet.h \
  proposer/proposer.h \
  proposer/proposer_logic.h \
  proposer/proposer_rpc.h \
  proposer/proposer_state.h \
  proposer/proposer_status.h \
  proposer/sync.h \
  proposer/waiter.h \
  protocol.h \
  settings.h \
  random.h \
  reverse_iterator.h \
  reverselock.h \
  rpc/blockchain.h \
  rpc/parameter_conversion.h \
  rpc/finalization.h \
  rpc/mining.h \
  rpc/proposing.h \
  rpc/protocol.h \
  rpc/safemode.h \
  rpc/server.h \
  rpc/staking.h \
  rpc/register.h \
  rpc/util.h \
  scheduler.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  script/ismine.h \
  snapshot/chainstate_iterator.h \
  snapshot/creator.h \
  snapshot/indexer.h \
  snapshot/initialization.h \
  snapshot/iterator.h \
  snapshot/messages.h \
  snapshot/p2p_processing.h \
  snapshot/params.h \
  snapshot/rpc_processing.h \
  snapshot/snapshot_index.h \
  snapshot/snapshot_validation.h \
  snapshot/state.h \
  staking/active_chain.h \
  staking/block_index_map.h \
  staking/block_validation_info.h \
  staking/block_validator.h \
  staking/coin.h \
  staking/legacy_validation_interface.h \
  staking/network.h \
  staking/proof_of_stake.h \
  staking/stake_validator.h \
  staking/staking_rpc.h \
  staking/stakingwallet.h \
  staking/transactionpicker.h \
  staking/validation_error.h \
  staking/validation_result.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/events.h \
  support/lockedpool.h \
  sync.h \
  sync_status.h \
  threadsafety.h \
  threadinterrupt.h \
  timedata.h \
  torcontrol.h \
  trit.h \
  txdb.h \
  txmempool.h \
  txpool.h \
  ufp64.h \
  ui_interface.h \
  undo.h \
  unilib/uninorms.h \
  unilib/utf8.h \
  util.h \
  util/scope_stopwatch.h \
  utilmoneystr.h \
  utiltime.h \
  utiltypetags.h \
  validation.h \
  validation_flags.h \
  validationinterface.h \
  versionbits.h \
  wallet/coincontrol.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/feebumper.h \
  wallet/fees.h \
  wallet/init.h \
  wallet/rpcaddressbook.h \
  wallet/rpcadmin.h \
  wallet/rpcmnemonic.h \
  wallet/rpcvalidator.h \
  wallet/rpcwallet.h \
  wallet/rpcwalletext.h \
  wallet/wallet.h \
  wallet/walletdb.h \
  wallet/walletutil.h \
  warnings.h \
  zmq/zmqabstractnotifier.h \
  zmq/zmqconfig.h\
  zmq/zmqnotificationinterface.h \
  zmq/zmqpublishnotifier.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh "$(abs_top_builddir)/src/obj/build.h" \
	  "$(abs_top_srcdir)"
libunite_util_a-clientversion.$(OBJEXT): obj/build.h

# server
libunite_server_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES) $(MINIUPNPC_CPPFLAGS) $(EVENT_CFLAGS) $(EVENT_PTHREADS_CFLAGS)
libunite_server_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_server_a_SOURCES = \
  addrdb.cpp \
  addrman.cpp \
  bloom.cpp \
  blockchain/blockchain_rpc.cpp \
  blockdb.cpp \
  blockencodings.cpp \
  chain.cpp \
  consensus/tx_verify.cpp \
  dbwrapper.cpp \
  esperanza/admincommand.cpp \
  esperanza/adminstate.cpp \
  esperanza/checkpoint.cpp \
  esperanza/checks.cpp \
  esperanza/finalizationstate.cpp \
  esperanza/finalizationstate_data.cpp \
  esperanza/validator.cpp \
  finalization/state_db.cpp \
  finalization/state_processor.cpp \
  finalization/state_repository.cpp \
  finalization/vote_recorder.cpp \
  httprpc.cpp \
  httpserver.cpp \
  iblt_params.cpp \
  init.cpp \
  injector.cpp \
  merkleblock.cpp \
  net.cpp \
  net_processing.cpp \
  noui.cpp \
  p2p/embargoman.cpp \
  p2p/embargoman_init.cpp \
  p2p/finalizer_commits_handler.cpp \
  p2p/finalizer_commits_handler_impl.cpp \
  p2p/finalizer_commits_types.cpp \
  p2p/graphene.cpp \
  p2p/graphene_hasher.cpp \
  p2p/graphene_receiver.cpp \
  p2p/graphene_sender.cpp \
  policy/fees.cpp \
  policy/policy.cpp \
  policy/rbf.cpp \
  rest.cpp \
  rpc/blockchain.cpp \
  rpc/finalization.cpp \
  rpc/mining.cpp \
  rpc/misc.cpp \
  rpc/net.cpp \
  rpc/rawtransaction.cpp \
  rpc/safemode.cpp \
  rpc/server.cpp \
  rpc/staking.cpp \
  script/sigcache.cpp \
  script/ismine.cpp \
  snapshot/chainstate_iterator.cpp \
  snapshot/creator.cpp \
  snapshot/indexer.cpp \
  snapshot/initialization.cpp \
  snapshot/iterator.cpp \
  snapshot/messages.cpp \
  snapshot/p2p_processing.cpp \
  snapshot/rpc_processing.cpp \
  snapshot/snapshot_index.cpp \
  snapshot/snapshot_validation.cpp \
  snapshot/state.cpp \
  staking/active_chain.cpp \
  staking/block_index_map.cpp \
  staking/block_validation_info.cpp \
  staking/block_validator.cpp \
  staking/coin.cpp \
  staking/legacy_validation_interface.cpp \
  staking/network.cpp \
  staking/proof_of_stake.cpp \
  staking/stake_validator.cpp \
  staking/staking_rpc.cpp \
  staking/validation_error.cpp \
  staking/validation_result.cpp \
  timedata.cpp \
  torcontrol.cpp \
  trit.cpp \
  txdb.cpp \
  txmempool.cpp \
  txpool.cpp \
  ui_interface.cpp \
  validation.cpp \
  validationinterface.cpp \
  versionbits.cpp \
  $(UNITE_CORE_H)

if ENABLE_ZMQ
libunite_zmq_a_CPPFLAGS = $(UNITE_INCLUDES) $(ZMQ_CFLAGS)
libunite_zmq_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_zmq_a_SOURCES = \
  zmq/zmqabstractnotifier.cpp \
  zmq/zmqnotificationinterface.cpp \
  zmq/zmqpublishnotifier.cpp
endif

# wallet: only linked when wallet enabled
libunite_wallet_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
libunite_wallet_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_wallet_a_SOURCES = \
  esperanza/init.cpp \
  esperanza/walletextension.cpp \
  esperanza/walletextension_deps.cpp \
  key/mnemonic/mnemonic.cpp \
  miner.cpp \
  proposer/block_builder.cpp \
  proposer/eligible_coin.cpp \
  proposer/multiwallet.cpp \
  proposer/proposer.cpp \
  proposer/proposer_logic.cpp \
  proposer/proposer_rpc.cpp \
  proposer/sync.cpp \
  proposer/waiter.cpp \
  staking/transactionpicker.cpp \
  rpc/proposing.cpp \
  unilib/uninorms.cpp \
  unilib/utf8.cpp \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/feebumper.cpp \
  wallet/fees.cpp \
  wallet/init.cpp \
  wallet/rpcadmin.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcmnemonic.cpp \
  wallet/rpcvalidator.cpp \
  wallet/rpcwallet.cpp \
  wallet/rpcwalletext.cpp \
  wallet/rpcaddressbook.cpp \
  wallet/wallet.cpp \
  wallet/walletdb.cpp \
  wallet/walletutil.cpp \
  $(UNITE_CORE_H)

if ENABLE_USBDEVICE
libunite_usbdevice_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES) -DHAVE_HIDAPI
libunite_usbdevice_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_usbdevice_a_CFLAGS = $(AM_CFLAGS) $(PIE_FLAGS) -DHAVE_HIDAPI
libunite_usbdevice_a_SOURCES = \
  extkey.cpp \
  usbdevice/debugdevice.cpp \
  usbdevice/debugdevice.h \
  usbdevice/ledgerapdu.cpp \
  usbdevice/ledgerapdu.h \
  usbdevice/ledgerdevice.cpp \
  usbdevice/ledgerdevice.h \
  usbdevice/ledger/btchipApdu.h \
  usbdevice/ledger/dongleCommHidHidapi.c \
  usbdevice/ledger/dongleCommHidHidapi.h \
  usbdevice/ledger/ledgerLayer.c \
  usbdevice/ledger/ledgerLayer.h \
  usbdevice/rpcusbdevice.cpp \
  usbdevice/rpcusbdevice.h \
  usbdevice/usbdevice.cpp \
  usbdevice/usbdevice.h
endif

# crypto primitives library
crypto_libunite_crypto_a_CPPFLAGS = $(AM_CPPFLAGS)
crypto_libunite_crypto_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
crypto_libunite_crypto_a_SOURCES = \
  crypto/aes.cpp \
  crypto/aes.h \
  crypto/chacha20.h \
  crypto/chacha20.cpp \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h

if USE_ASM
crypto_libunite_crypto_a_SOURCES += crypto/sha256_sse4.cpp
endif

# consensus: shared between all executables that validate any consensus rules.
libunite_consensus_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
libunite_consensus_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_consensus_a_SOURCES = \
  amount.h \
  arith_uint256.cpp \
  arith_uint256.h \
  consensus/ltor.h \
  consensus/ltor.cpp \
  consensus/merkle.cpp \
  consensus/merkle.h \
  consensus/params.h \
  consensus/validation.h \
  hash.cpp \
  hash.h \
  prevector.h \
  primitives/block.cpp \
  primitives/block.h \
  primitives/transaction.cpp \
  primitives/transaction.h \
  primitives/txtype.h \
  pubkey.cpp \
  pubkey.h \
  script/uniteconsensus.cpp \
  script/interpreter.cpp \
  script/interpreter.h \
  script/script.cpp \
  script/script.h \
  script/script_error.cpp \
  script/script_error.h \
  serialize.h \
  tinyformat.h \
  uint256.cpp \
  uint256.h \
  utilstrencodings.cpp \
  utilstrencodings.h \
  version.h

# common: shared between unit-e and non-server tools
libunite_common_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
libunite_common_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_common_a_SOURCES = \
  base58.cpp \
  bech32.cpp \
  blockchain/blockchain_behavior.cpp \
  blockchain/blockchain_custom_parameters.cpp \
  blockchain/blockchain_genesis.cpp \
  blockchain/blockchain_parameters.cpp \
  blockchain/testnet_funds.cpp \
  blockchain/regtest_funds.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  esperanza/finalizationparams.cpp \
  esperanza/vote.cpp \
  key.cpp \
  keystore.cpp \
  netaddress.cpp \
  netbase.cpp \
  policy/feerate.cpp \
  protocol.cpp \
  scheduler.cpp \
  script/sign.cpp \
  script/standard.cpp \
  snapshot/messages.cpp \
  snapshot/iterator.cpp \
  snapshot/indexer.cpp \
  ufp64.cpp \
  warnings.cpp \
  $(UNITE_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libunite_util_a_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
libunite_util_a_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
libunite_util_a_SOURCES = \
  support/lockedpool.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  fs.cpp \
  random.cpp \
  rpc/parameter_conversion.cpp \
  rpc/protocol.cpp \
  rpc/util.cpp \
  settings.cpp \
  support/cleanse.cpp \
  sync.cpp \
  threadinterrupt.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(UNITE_CORE_H)

if GLIBC_BACK_COMPAT
libunite_util_a_SOURCES += compat/glibc_compat.cpp
endif

nodist_libunite_util_a_SOURCES = $(srcdir)/obj/build.h
#

# unit-e binary #
unit_e_SOURCES = unit-e.cpp
unit_e_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
unit_e_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
unit_e_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
unit_e_SOURCES += unit-e-res.rc
endif

unit_e_LDADD = \
  $(LIBUNITE_SERVER) \
  $(LIBUNITE_UTIL) \
  $(LIBUNITE_USBDEVICE) \
  $(LIBUNITE_WALLET) \
  $(LIBUNITE_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBUNITE_ZMQ) \
  $(LIBUNITE_CONSENSUS) \
  $(LIBUNITE_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBLEVELDB_SSE42) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

unit_e_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(EVENT_PTHREADS_LIBS) $(EVENT_LIBS) $(ZMQ_LIBS) \
  $(HIDAPI_LIBS) $(USB_LIBS)

# unit-e-cli binary #
unit_e_cli_SOURCES = unit-e-cli.cpp
unit_e_cli_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES) $(EVENT_CFLAGS)
unit_e_cli_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
unit_e_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
unit_e_cli_SOURCES += unit-e-cli-res.rc
endif

unit_e_cli_LDADD = \
  $(LIBUNIVALUE) \
  $(LIBUNITE_UTIL) \
  $(LIBUNITE_CRYPTO)

unit_e_cli_LDADD += $(BOOST_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(EVENT_LIBS) \
  $(HIDAPI_LIBS) $(USB_LIBS)
#

# unit-e-tx binary #
unit_e_tx_SOURCES = unit-e-tx.cpp
unit_e_tx_CPPFLAGS = $(AM_CPPFLAGS) $(UNITE_INCLUDES)
unit_e_tx_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)
unit_e_tx_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
unit_e_tx_SOURCES += unit-e-tx-res.rc
endif

unit_e_tx_LDADD = \
  $(LIBUNITE_COMMON) \
  $(LIBUNIVALUE) \
  $(LIBUNITE_UTIL) \
  $(LIBUNITE_CONSENSUS) \
  $(LIBUNITE_CRYPTO) \
  $(LIBSECP256K1)

unit_e_tx_LDADD += $(BOOST_LIBS) $(CRYPTO_LIBS) \
  $(HIDAPI_LIBS) $(USB_LIBS)
#

# uniteconsensus library #
if BUILD_UNITE_LIBS
include_HEADERS = script/uniteconsensus.h
libuniteconsensus_la_SOURCES = $(crypto_libunite_crypto_a_SOURCES) $(libunite_consensus_a_SOURCES)

if GLIBC_BACK_COMPAT
  libuniteconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libuniteconsensus_la_LDFLAGS = $(AM_LDFLAGS) -no-undefined $(RELDFLAGS)
libuniteconsensus_la_LIBADD = $(LIBSECP256K1)
libuniteconsensus_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/obj -I$(srcdir)/secp256k1/include -DBUILD_UNITE_INTERNAL
libuniteconsensus_la_CXXFLAGS = $(AM_CXXFLAGS) $(PIE_FLAGS)

endif
#

CTAES_DIST =  crypto/ctaes/bench.c
CTAES_DIST += crypto/ctaes/ctaes.c
CTAES_DIST += crypto/ctaes/ctaes.h
CTAES_DIST += crypto/ctaes/README.md
CTAES_DIST += crypto/ctaes/test.c

CLEANFILES = $(EXTRA_LIBRARIES)

CLEANFILES += *.gcda *.gcno
CLEANFILES += compat/*.gcda compat/*.gcno
CLEANFILES += consensus/*.gcda consensus/*.gcno
CLEANFILES += crypto/*.gcda crypto/*.gcno
CLEANFILES += policy/*.gcda policy/*.gcno
CLEANFILES += primitives/*.gcda primitives/*.gcno
CLEANFILES += script/*.gcda script/*.gcno
CLEANFILES += support/*.gcda support/*.gcno
CLEANFILES += univalue/*.gcda univalue/*.gcno
CLEANFILES += wallet/*.gcda wallet/*.gcno
CLEANFILES += wallet/test/*.gcda wallet/test/*.gcno
CLEANFILES += zmq/*.gcda zmq/*.gcno
CLEANFILES += obj/build.h

EXTRA_DIST = $(CTAES_DIST)


config/unite-config.h: config/stamp-h1
	@$(MAKE) -C $(top_builddir) $(subdir)/$(@)
config/stamp-h1: $(top_srcdir)/$(subdir)/config/unite-config.h.in $(top_builddir)/config.status
	$(AM_V_at)$(MAKE) -C $(top_builddir) $(subdir)/$(@)
$(top_srcdir)/$(subdir)/config/unite-config.h.in:  $(am__configure_deps)
	$(AM_V_at)$(MAKE) -C $(top_srcdir) $(subdir)/config/unite-config.h.in

clean-local:
	-$(MAKE) -C secp256k1 clean
	-$(MAKE) -C univalue clean
	-rm -f leveldb/*/*.gcda leveldb/*/*.gcno leveldb/helpers/memenv/*.gcda leveldb/helpers/memenv/*.gcno
	-rm -f config.h
	-rm -rf test/__pycache__

.rc.o:
	@test -f $(WINDRES)
	## FIXME: How to get the appropriate modulename_CPPFLAGS in here?
	$(AM_V_GEN) $(WINDRES) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(CPPFLAGS) -DWINDRES_PREPROC -i $< -o $@

check-symbols: $(bin_PROGRAMS)
if GLIBC_BACK_COMPAT
	@echo "Checking glibc back compat..."
	$(AM_V_at) READELF=$(READELF) CPPFILT=$(CPPFILT) $(top_srcdir)/contrib/devtools/symbol-check.py < $(bin_PROGRAMS)
endif

check-security: $(bin_PROGRAMS)
if HARDEN
	@echo "Checking binary security..."
	$(AM_V_at) READELF=$(READELF) OBJDUMP=$(OBJDUMP) $(top_srcdir)/contrib/devtools/security-check.py < $(bin_PROGRAMS)
endif

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(<D) $<

if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include
endif

if ENABLE_BENCH
include Makefile.bench.include
endif
