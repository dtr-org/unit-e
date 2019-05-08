#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check for circular dependencies

export LC_ALL=C

EXPECTED_CIRCULAR_DEPENDENCIES=(
    "blockchain/blockchain_behavior -> blockchain/blockchain_custom_parameters -> blockchain/blockchain_genesis -> blockchain/blockchain_behavior"
    "blockchain/blockchain_behavior -> blockchain/blockchain_parameters -> settings -> key_io -> chainparams -> blockchain/blockchain_behavior"
    "blockchain/blockchain_genesis -> blockchain/blockchain_parameters -> blockchain/blockchain_genesis"
    "blockchain/blockchain_parameters -> settings -> key_io -> chainparams -> blockchain/blockchain_parameters"
    "blockdb -> validation -> injector -> blockdb"
    "blockencodings -> injector -> p2p/graphene_receiver -> net_processing -> blockencodings"
    "blockencodings -> injector -> p2p/graphene_sender -> blockencodings"
    "chainparamsbase -> util -> chainparamsbase"
    "coins -> snapshot/iterator -> coins"
    "coins -> snapshot/messages -> coins"
    "core_io -> script/sign -> policy/policy -> validation -> core_io"
    "consensus/merkle -> primitives/block -> consensus/merkle"
    "esperanza/checks -> txmempool -> esperanza/checks"
    "esperanza/checks -> validation -> esperanza/checks"
    "esperanza/walletextension -> wallet/wallet -> esperanza/walletextension"
    "finalization/state_db -> validation -> injector -> finalization/state_db"
    "finalization/state_processor -> finalization/state_repository -> finalization/state_processor"
    "finalization/state_repository -> validation -> injector -> finalization/state_repository"
    "httprpc -> httpserver -> init -> httprpc"
    "httpserver -> init -> httpserver"
    "index/txindex -> validation -> index/txindex"
    "injector -> p2p/graphene_receiver -> net_processing -> injector"
    "injector -> p2p/graphene_receiver -> validation -> injector"
    "injector -> staking/transactionpicker -> miner -> injector"
    "injector -> txpool -> txmempool -> injector"
    "keystore -> script/ismine -> keystore"
    "keystore -> script/sign -> keystore"
    "net_processing -> p2p/graphene -> txpool -> net_processing"
    "p2p/finalizer_commits_handler -> p2p/finalizer_commits_handler_impl -> p2p/finalizer_commits_handler"
    "policy/fees -> policy/policy -> validation -> policy/fees"
    "policy/fees -> txmempool -> policy/fees"
    "policy/policy -> validation -> policy/policy"
    "policy/rbf -> txmempool -> validation -> policy/rbf"
    "rpc/rawtransaction -> wallet/rpcwallet -> rpc/rawtransaction"
    "snapshot/snapshot_index -> txdb -> snapshot/snapshot_index"
    "staking/validation_error -> staking/validation_result -> staking/validation_error"
    "sync -> util -> sync"
    "txmempool -> validation -> txmempool"
    "txmempool -> validation -> validationinterface -> txmempool"
    "usbdevice/debugdevice -> usbdevice/usbdevice -> usbdevice/debugdevice"
    "usbdevice/ledgerdevice -> usbdevice/usbdevice -> usbdevice/ledgerdevice"
    "usbdevice/usbdevice -> wallet/wallet -> usbdevice/usbdevice"
    "validation -> validationinterface -> validation"
    "wallet/coincontrol -> wallet/wallet -> wallet/coincontrol"
    "wallet/fees -> wallet/wallet -> wallet/fees"
    "wallet/rpcwallet -> wallet/wallet -> wallet/rpcwallet"
    "wallet/wallet -> wallet/walletdb -> wallet/wallet"
)

EXIT_CODE=0

CIRCULAR_DEPENDENCIES=()

IFS=$'\n'
for CIRC in $(cd src && ../contrib/devtools/circular-dependencies.py {*,*/*,*/*/*}.{h,cpp} | sed -e 's/^Circular dependency: //'); do
    CIRCULAR_DEPENDENCIES+=($CIRC)
    IS_EXPECTED_CIRC=0
    for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_EXPECTED_CIRC} == 0 ]]; then
        echo "A new circular dependency in the form of \"${CIRC}\" appears to have been introduced."
        echo
        EXIT_CODE=1
    fi
done

for EXPECTED_CIRC in "${EXPECTED_CIRCULAR_DEPENDENCIES[@]}"; do
    IS_PRESENT_EXPECTED_CIRC=0
    for CIRC in "${CIRCULAR_DEPENDENCIES[@]}"; do
        if [[ "${CIRC}" == "${EXPECTED_CIRC}" ]]; then
            IS_PRESENT_EXPECTED_CIRC=1
            break
        fi
    done
    if [[ ${IS_PRESENT_EXPECTED_CIRC} == 0 ]]; then
        echo "Good job! The circular dependency \"${EXPECTED_CIRC}\" is no longer present."
        echo "Please remove it from EXPECTED_CIRCULAR_DEPENDENCIES in $0"
        echo "to make sure this circular dependency is not accidentally reintroduced."
        echo
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
