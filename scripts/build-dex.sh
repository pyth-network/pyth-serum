#!/usr/bin/env bash

set -eu

if [[ ! -v SERUM_DEX ]]
then
  _THIS_DIR="$( dirname "${BASH_SOURCE[0]}" )"
  SERUM_DEX="$( cd "${_THIS_DIR}/../.." && pwd )/serum-dex"
fi

if ! which cargo 2> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x
cd "${SERUM_DEX}/dex"
cargo +bpf build "$@"
sha256sum -b target/*/*/*.so
