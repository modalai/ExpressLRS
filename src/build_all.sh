#!/usr/bin/env bash

set -euo pipefail

MODE="release"
REVISION=""
ENCRYPT_KEY=""
BUILD_FACTORY=0
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR"

usage()
{
    echo "Build a ModalAI ExpressLRS artifact set."
    echo "Usage: ./build_all.sh [--release|--jlink|--hwil|--all] [-v REVISION] [-e KEY] [--factory]"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help) usage; exit 0 ;;
        --release) MODE="release"; shift ;;
        --jlink) MODE="jlink"; shift ;;
        --hwil) MODE="hwil"; shift ;;
        --all) MODE="all"; shift ;;
        --factory) BUILD_FACTORY=1; shift ;;
        -v)
            [ "$#" -ge 2 ] || { echo "The -v option requires a revision." >&2; exit 1; }
            REVISION="$2"
            shift 2
            ;;
        -e)
            [ "$#" -ge 2 ] || { echo "The -e option requires a key." >&2; exit 1; }
            ENCRYPT_KEY="$2"
            shift 2
            ;;
        *) echo "Unknown option: $1" >&2; usage; exit 1 ;;
    esac
done

release_targets=(m0184_rx m0184_tx m0193_rx m0193_tx)
jlink_targets=(m0184_rx_jlink m0184_tx_jlink m0193_rx_jlink m0193_tx_jlink)
hwil_targets=(m0184_hwil_rx m0184_hwil_tx m0193_hwil_rx m0193_hwil_tx)

case "$MODE" in
    release) targets=("${release_targets[@]}") ;;
    jlink) targets=("${jlink_targets[@]}") ;;
    hwil) targets=("${hwil_targets[@]}") ;;
    all) targets=("${release_targets[@]}" "${jlink_targets[@]}" "${hwil_targets[@]}") ;;
esac

for target in "${targets[@]}"; do
    args=(-t "$target")
    [ -z "$REVISION" ] || args+=(-v "$REVISION")
    [ -z "$ENCRYPT_KEY" ] || args+=(-e "$ENCRYPT_KEY")
    if [ "$BUILD_FACTORY" -eq 1 ]; then
        case "$target" in
            m0184_rx|m0184_tx|m0193_rx|m0193_tx) args+=(--factory) ;;
        esac
    fi
    "$SCRIPT_DIR/build.sh" "${args[@]}"
done
