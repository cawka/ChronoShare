#!/usr/bin/env bash
set -e

JDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
source "$JDIR"/util.sh

set -x

if has OSX $NODE_LABELS; then
    brew update
    brew upgrade
    brew install boost pkg-config cryptopp openssl log4cxx protobuf tinyxml qt5
    brew cleanup
fi

if has Ubuntu $NODE_LABELS; then
    sudo apt-get -qq update
    sudo apt-get -qq install build-essential pkg-config libboost-all-dev \
                             libcrypto++-dev libsqlite3-dev libssl-dev \
                             libpcap-dev \
                             liblog4cxx10-dev protobuf-compiler libprotobuf-dev libtinyxml-dev qt5-default
fi
