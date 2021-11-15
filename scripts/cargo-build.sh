#!/usr/bin/env bash

set -eu

BUILD_DIR="$( cd "${1:-.}" && pwd )"

# Build offline by default after building Docker.
# Override with CARGO_NET_OFFLINE=false.
export CARGO_NET_OFFLINE="${CARGO_NET_OFFLINE:-true}"

if ! which cargo &> /dev/null
then
  # shellcheck disable=SC1090
  source "${CARGO_HOME:-$HOME/.cargo}/env"
fi

set -x
cd "${BUILD_DIR}"
cargo +bpf build "${@:2}"
sha256sum -b target/*/*/*.so
