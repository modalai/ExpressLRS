#!/bin/bash

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
TARGET=""
ENCRYPT_KEY=""

print_usage () {
	echo ""
	echo "Build script for buidling ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encrpytion of firmware (pass in the key)"
    echo -e "   t) Target to build (m0184, r9mini)"
    echo -e "   v) Version of firmware being built"
}

while getopts "e:t:v:" opt; do
    case $opt in
        "h")
            print_usage
            exit 0
            ;;
        "e")
            ENCRYPT_KEY=${OPTARG}
            ENCRYPT=1
            echo "Using encryption"
            ;;
        "v")
            MODALAI_VERSION=${OPTARG}
            echo "Using version #: $MODALAI_VERSION"
            ;;
        "t") 
            TARGET=${OPTARG}
            echo "Building target: $TARGET"
            ;;
        *)
            echo "invalid option $arg"
            print_usage
            exit 1
            ;;
    esac
done

case $TARGET in
    "m0184_rx"|"M0184")
        TARGET="MODALAI_M0184_RX_via_UART"
        FW="ModalAI_M0184_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0184_tx"|"M0184_TX")
        TARGET="MODALAI_M0184_TX_via_UART"
        FW="ModalAI_M0184_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0184_hwil_tx"|"M0184_HWIL_TX")
        TARGET="MODALAI_M0184_HWIL_TX_via_UART"
        FW="ModalAI_M0184_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0184_hwil_rx"|"M0184_HWIL_RX")
        TARGET="MODALAI_M0184_HWIL_RX_via_UART"
        FW="ModalAI_M0184_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0193_rx"|"M0193")
        TARGET="MODALAI_M0193_RX_via_UART"
        FW="ModalAI_M0193_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0193_tx"|"M0193_TX")
        TARGET="MODALAI_M0193_TX_via_UART"
        FW="ModalAI_M0193_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0193_hwil_tx"|"M0193_HWIL_TX")
        TARGET="MODALAI_M0193_HWIL_TX_via_UART"
        FW="ModalAI_M0193_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0193_hwil_rx"|"M0193_HWIL_RX")
        TARGET="MODALAI_M0193_HWIL_RX_via_UART"
        FW="ModalAI_M0193_RX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
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

# Build application
export PLATFORMIO_BUILD_FLAGS="-DRegulatory_Domain_FCC_915"
if [[ $TARGET == Unified_ESP* ]]; then  
    echo "" | pio run -e $TARGET # echo "" to use Generic 900.json layout file
else
    pio run --environment $TARGET # -v > build_output.txt
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
mkdir -p $OUTDIR
cp $BUILD_DIR/$FW $OUTDIR
