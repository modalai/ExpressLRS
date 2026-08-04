#!/usr/bin/env bash

set -euo pipefail

BASE_VERSION="4.1.0"
MODALAI_REVISION=""
TARGET_ALIAS=""
ENCRYPT_KEY=""
BUILD_FACTORY=0
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

cd "$SCRIPT_DIR"

usage()
{
    echo "Build one ModalAI ExpressLRS v4 artifact."
    echo "Usage: ./build.sh -t TARGET [-v REVISION] [-e KEY] [--factory]"
    echo "Use a target such as m0184_rx, m0193_tx_jlink, or m0184_hwil_rx."
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -t)
            [ "$#" -ge 2 ] || { echo "The -t option requires a target." >&2; exit 1; }
            TARGET_ALIAS="$2"
            shift 2
            ;;
        -v)
            [ "$#" -ge 2 ] || { echo "The -v option requires a revision." >&2; exit 1; }
            MODALAI_REVISION="$2"
            shift 2
            ;;
        -e)
            [ "$#" -ge 2 ] || { echo "The -e option requires a key." >&2; exit 1; }
            ENCRYPT_KEY="$2"
            shift 2
            ;;
        --factory)
            BUILD_FACTORY=1
            shift
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
done

[ -n "$TARGET_ALIAS" ] || { echo "Select a target with -t." >&2; exit 1; }

case "$TARGET_ALIAS" in
    m0184_rx) ENVIRONMENT="MODALAI_M0184_RX_via_UART"; PRODUCT="MODALAI_M0184_RX"; BOOTLOADER_ENV="MRX" ;;
    m0184_tx) ENVIRONMENT="MODALAI_M0184_TX_via_UART"; PRODUCT="MODALAI_M0184_TX"; BOOTLOADER_ENV="MTX" ;;
    m0193_rx) ENVIRONMENT="MODALAI_M0193_RX_via_UART"; PRODUCT="MODALAI_M0193_RX"; BOOTLOADER_ENV="MRX" ;;
    m0193_tx) ENVIRONMENT="MODALAI_M0193_TX_via_UART"; PRODUCT="MODALAI_M0193_TX"; BOOTLOADER_ENV="MTX" ;;
    m0184_rx_jlink) ENVIRONMENT="MODALAI_M0184_RX_via_JLINK"; PRODUCT="MODALAI_M0184_RX_JLINK"; BOOTLOADER_ENV="" ;;
    m0184_tx_jlink) ENVIRONMENT="MODALAI_M0184_TX_via_JLINK"; PRODUCT="MODALAI_M0184_TX_JLINK"; BOOTLOADER_ENV="" ;;
    m0193_rx_jlink) ENVIRONMENT="MODALAI_M0193_RX_via_JLINK"; PRODUCT="MODALAI_M0193_RX_JLINK"; BOOTLOADER_ENV="" ;;
    m0193_tx_jlink) ENVIRONMENT="MODALAI_M0193_TX_via_JLINK"; PRODUCT="MODALAI_M0193_TX_JLINK"; BOOTLOADER_ENV="" ;;
    m0184_hwil_rx) ENVIRONMENT="MODALAI_M0184_HWIL_RX_via_UART"; PRODUCT="MODALAI_M0184_HWIL_RX"; BOOTLOADER_ENV="" ;;
    m0184_hwil_tx) ENVIRONMENT="MODALAI_M0184_HWIL_TX_via_UART"; PRODUCT="MODALAI_M0184_HWIL_TX"; BOOTLOADER_ENV="" ;;
    m0193_hwil_rx) ENVIRONMENT="MODALAI_M0193_HWIL_RX_via_UART"; PRODUCT="MODALAI_M0193_HWIL_RX"; BOOTLOADER_ENV="" ;;
    m0193_hwil_tx) ENVIRONMENT="MODALAI_M0193_HWIL_TX_via_UART"; PRODUCT="MODALAI_M0193_HWIL_TX"; BOOTLOADER_ENV="" ;;
    *)
        echo "Unknown ModalAI target: $TARGET_ALIAS" >&2
        exit 1
        ;;
esac

if [ -z "$MODALAI_REVISION" ]; then
    highest_tag="$(git tag --list "${BASE_VERSION}.*" | sort -V | tail -n 1)"
    if [ -n "$highest_tag" ]; then
        MODALAI_REVISION="${highest_tag##*.}"
    else
        MODALAI_REVISION=0
    fi
fi

case "$MODALAI_REVISION" in
    ''|*[!0-9]*) echo "The ModalAI revision must be a nonnegative integer." >&2; exit 1 ;;
esac
if [ "$MODALAI_REVISION" -gt 255 ]; then
    echo "The ModalAI revision must not exceed 255." >&2
    exit 1
fi

if [ "$BUILD_FACTORY" -eq 1 ] && [ -z "$BOOTLOADER_ENV" ]; then
    echo "Factory images require a production UART target." >&2
    exit 1
fi

RELEASE_VERSION="${BASE_VERSION}.${MODALAI_REVISION}"
BUILD_DIR=".pio/build/${ENVIRONMENT}"
ARTIFACT_DIR="artifacts/${RELEASE_VERSION}/${ENVIRONMENT}"
ARTIFACT_NAME="${PRODUCT}-${RELEASE_VERSION}.bin"

echo "Build ${ENVIRONMENT} as ${RELEASE_VERSION}."
MODALAI_RELEASE_VERSION="$RELEASE_VERSION" pio run -e "$ENVIRONMENT"

SOURCE_BIN="${BUILD_DIR}/firmware.bin"
[ -f "$SOURCE_BIN" ] || { echo "The build did not create ${SOURCE_BIN}." >&2; exit 1; }

mkdir -p "$ARTIFACT_DIR"
if [ -n "$ENCRYPT_KEY" ]; then
    command -v stm32-encrypt >/dev/null || { echo "Install stm32-encrypt before encrypted builds." >&2; exit 1; }
    stm32-encrypt "$SOURCE_BIN" "${ARTIFACT_DIR}/${ARTIFACT_NAME}" "$ENCRYPT_KEY"
else
    install -m 0644 "$SOURCE_BIN" "${ARTIFACT_DIR}/${ARTIFACT_NAME}"
fi
md5sum "${ARTIFACT_DIR}/${ARTIFACT_NAME}"

if [ "$BUILD_FACTORY" -eq 1 ]; then
    echo "Build the ${BOOTLOADER_ENV} bootloader."
    pio run -d bootloader/src -e "$BOOTLOADER_ENV"
    BOOTLOADER="bootloader/src/binaries/${BOOTLOADER_ENV,,}_bootloader.bin"
    [ -f "$BOOTLOADER" ] || { echo "The bootloader does not exist: ${BOOTLOADER}." >&2; exit 1; }

    bootloader_size="$(stat -c%s "$BOOTLOADER")"
    application_size="$(stat -c%s "$SOURCE_BIN")"
    application_offset=$((0x2400))
    maximum_application_size=98304
    [ "$bootloader_size" -le "$application_offset" ] || { echo "The bootloader exceeds the application offset." >&2; exit 1; }
    [ "$application_size" -le "$maximum_application_size" ] || { echo "The application exceeds the flash partition." >&2; exit 1; }

    factory_dir="artifacts/${RELEASE_VERSION}/factory/${ENVIRONMENT}"
    factory_bin="${BUILD_DIR}/${PRODUCT}-${RELEASE_VERSION}-factory.bin"
    factory_srec="${factory_dir}/${PRODUCT}-${RELEASE_VERSION}-factory.srec"
    padding_size=$((application_offset - bootloader_size))
    mkdir -p "$factory_dir"
    {
        command cat "$BOOTLOADER"
        dd if=/dev/zero bs=1 count="$padding_size" status=none | tr '\000' '\377'
        command cat "$SOURCE_BIN"
    } > "$factory_bin"
    pio pkg exec -p toolchain-gccarmnoneeabi -c \
        "arm-none-eabi-objcopy -I binary -O srec --change-addresses=0x08000000 ${factory_bin} ${factory_srec}"
    rm -f "$factory_bin"
    md5sum "$factory_srec"
fi
