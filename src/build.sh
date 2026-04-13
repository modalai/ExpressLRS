#!/usr/bin/env bash

# Versioning info: 
# https://www.expresslrs.org/quick-start/receivers/firmware-version/#receiver-firmware-version 
# https://semver.org/
MAJOR_VERSION=3
MINOR_VERSION=5
PATCH_VERSION=3

# Find the highest MODALAI_VERSION for this MAJOR.MINOR.PATCH combination
MODALAI_TAGS=$(git tag --list | grep "^${MAJOR_VERSION}\.${MINOR_VERSION}\.${PATCH_VERSION}\." | sort -V)
if [ -n "$MODALAI_TAGS" ]; then
    # Get the last (highest) tag and extract the MODALAI_VERSION
    HIGHEST_MODALAI_TAG=$(echo "$MODALAI_TAGS" | tail -1)
    export MODALAI_VERSION=${HIGHEST_MODALAI_TAG##*.}
    echo "Found MODALAI tags for ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}, using MODALAI_VERSION=$MODALAI_VERSION"
else
    export MODALAI_VERSION=0
    echo "No MODALAI tags found for ${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}, using MODALAI_VERSION=0"
fi

FORMAT_MAJOR=$(printf %02d $MAJOR_VERSION)
FORMAT_MINOR=$(printf %02d $MINOR_VERSION)
FORMAT_PATCH=$(printf %02d $PATCH_VERSION)
export ELRS_VER="0x${FORMAT_MAJOR}${FORMAT_MINOR}${FORMAT_PATCH}00"
ENCRYPT=0
FACTORY=0
TARGET=""
ENCRYPT_KEY=""
FACTORY_BOOT_ENV=""
FACTORY_BOOT_BIN=""
FACTORY_APP_OFFSET=0x2400
FACTORY_APP_MAX_SIZE=98304
FACTORY_FLASH_SIZE=$((FACTORY_APP_OFFSET + FACTORY_APP_MAX_SIZE))

print_usage () {
	echo ""
	echo "Build script for building ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encryption of firmware (pass in the key)"
    echo -e "   t) Target to build (m0184_rx, m0193_rx, m0193_tx, BETAFPV_900_RX)"
    echo -e "   v) Version of firmware being built"
    echo -e "   --factory Build a factory image for ModalAI STM32 targets"
}

while [ $# -gt 0 ]; do
    case "$1" in
        "-h"|"--help")
            print_usage
            exit 0
            ;;
        "-e")
            if [ $# -lt 2 ]; then
                echo "Missing value for -e"
                print_usage
                exit 1
            fi
            ENCRYPT_KEY=${2}
            ENCRYPT=1
            echo "Using encryption"
            shift 2
            ;;
        "-v")
            if [ $# -lt 2 ]; then
                echo "Missing value for -v"
                print_usage
                exit 1
            fi
            MODALAI_VERSION=${2}
            echo "Using version #: $MODALAI_VERSION"
            shift 2
            ;;
        "-t")
            if [ $# -lt 2 ]; then
                echo "Missing value for -t"
                print_usage
                exit 1
            fi
            TARGET=${2}
            echo "Building target: $TARGET"
            shift 2
            ;;
        "--factory")
            FACTORY=1
            echo "Building factory image"
            shift
            ;;
        *)
            echo "invalid option $1"
            print_usage
            exit 1
            ;;
    esac
done

case $TARGET in
    "m0184_rx"|"M0184")
        TARGET="MODALAI_M0184_RX_via_UART"
        FW="MODALAI_M0184_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MRX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mrx_bootloader.bin"
        ;;
    "m0184_tx"|"M0184_TX")
        TARGET="MODALAI_M0184_TX_via_UART"
        FW="MODALAI_M0184_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MTX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mtx_bootloader.bin"
        ;;
    "m0184_hwil_tx"|"M0184_HWIL_TX")
        TARGET="MODALAI_M0184_HWIL_TX_via_UART"
        FW="MODALAI_M0184_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MTX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mtx_bootloader.bin"
        ;;
    "m0184_hwil_rx"|"M0184_HWIL_RX")
        TARGET="MODALAI_M0184_HWIL_RX_via_UART"
        FW="MODALAI_M0184_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MRX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mrx_bootloader.bin"
        ;;
    "m0193_rx"|"M0193")
        TARGET="MODALAI_M0193_RX_via_UART"
        FW="MODALAI_M0193_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MRX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mrx_bootloader.bin"
        ;;
    "m0193_tx"|"M0193_TX")
        TARGET="MODALAI_M0193_TX_via_UART"
        FW="MODALAI_M0193_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MTX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mtx_bootloader.bin"
        ;;
    "m0193_hwil_tx"|"M0193_HWIL_TX")
        TARGET="MODALAI_M0193_HWIL_TX_via_UART"
        FW="MODALAI_M0193_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MTX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mtx_bootloader.bin"
        ;;
    "m0193_hwil_rx"|"M0193_HWIL_RX")
        TARGET="MODALAI_M0193_HWIL_RX_via_UART"
        FW="MODALAI_M0193_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        FACTORY_BOOT_ENV="MRX"
        FACTORY_BOOT_BIN="bootloader/src/binaries/mrx_bootloader.bin"
        ;;
    "r9mini"|"R9Mini")
        # R9Mini support from us stops at fw version 3.2.1... last version from ModalAI was 3.2.1.3
        TARGET="MODALAI_Frsky_RX_R9MM_R9MINI_via_UART"
        MINOR_VERSON=2
        PATCH_VERSION=1
        FW="FrSky_R9Mini-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "iFlight")
        TARGET="Unified_ESP32_900_TX_via_UART"
        FW="iFlight-900-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "BETAFPV_900_RX")
        TARGET="Unified_ESP8285_900_RX_via_UART"
        FW="BETAFPV_900_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    *)
        echo "Missing Target to build for!"
        exit
        ;;
esac

if [ "$FACTORY" -eq 1 ] && [ -z "$FACTORY_BOOT_ENV" ]; then
    echo "--factory is only supported for ModalAI STM32 targets"
    exit 1
fi

# Build application
# export PLATFORMIO_BUILD_FLAGS="-DRegulatory_Domain_FCC_915" # not needed since user_defines.txt is used
if [[ $TARGET == Unified_ESP* ]]; then  
    #if FW BETAFPV_900_RX
    if [[ $FW == BETAFPV_900_RX* ]]; then
        export ELRS_BOARD_CONFIG=betafpv.rx_900.plain
    fi
    pio run -e $TARGET
else
    pio run -e $TARGET # -v > build_output.txt
fi

BUILD_DIR=".pio/build/$TARGET"
md5sum $BUILD_DIR/firmware.bin

if [ "$ENCRYPT" -eq 1 ]; then 
    # Encrypt application 
    echo "Encrypting application"
    stm32-encrypt $BUILD_DIR/firmware.bin $BUILD_DIR/$FW $ENCRYPT_KEY
else
    echo "Leaving application un-encrypted"
    cp $BUILD_DIR/firmware.bin $BUILD_DIR/$FW
fi

# Print out md5sum so we can verify what we push onto target matches what we just built and encrypted here
md5sum $BUILD_DIR/$FW

# Copy/Push ELRS FW bin to desired location on voxl2
# adb push $BUILD_DIR/$FW /usr/share/modalai/voxl-elrs/firmware/rx
OUTDIR=artifacts/$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION/$TARGET
FACTORY_OUTDIR=artifacts/$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION/factory/$TARGET
mkdir -p $OUTDIR
cp $BUILD_DIR/$FW $OUTDIR

if [ "$FACTORY" -eq 1 ]; then
    APP_BIN="$BUILD_DIR/firmware.bin"
    FACTORY_FW="${FW%.bin}-factory.bin"
    FACTORY_PATH="$FACTORY_OUTDIR/$FACTORY_FW"
    APP_OFFSET_DEC=$((FACTORY_APP_OFFSET))

    echo "Building bootloader: $FACTORY_BOOT_ENV"
    pio run -d bootloader/src -e "$FACTORY_BOOT_ENV"

    if [ ! -f "$FACTORY_BOOT_BIN" ]; then
        echo "Missing bootloader binary: $FACTORY_BOOT_BIN"
        exit 1
    fi

    BOOTLOADER_SIZE=$(stat -c%s "$FACTORY_BOOT_BIN")
    APP_SIZE=$(stat -c%s "$APP_BIN")

    if [ "$BOOTLOADER_SIZE" -gt "$APP_OFFSET_DEC" ]; then
        echo "Bootloader binary is too large for app offset 0x$(printf '%X' "$APP_OFFSET_DEC")"
        exit 1
    fi

    if [ $((APP_OFFSET_DEC + APP_SIZE)) -gt "$FACTORY_FLASH_SIZE" ]; then
        echo "Factory image exceeds available flash"
        exit 1
    fi

    PADDING_SIZE=$((APP_OFFSET_DEC - BOOTLOADER_SIZE))

    mkdir -p "$FACTORY_OUTDIR"
    {
        cat "$FACTORY_BOOT_BIN"
        if [ "$PADDING_SIZE" -gt 0 ]; then
            dd if=/dev/zero bs=1 count="$PADDING_SIZE" status=none | tr '\000' '\377'
        fi
        cat "$APP_BIN"
    } > "$FACTORY_PATH"

    echo "Factory image created: $FACTORY_PATH"
    md5sum "$FACTORY_PATH"
fi
