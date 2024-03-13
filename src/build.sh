#!/bin/bash

VERSION=0
if [ $# -gt 0 ]; then 
    ENCRYPT=$1
else 
    ENCRYPT=0
fi

BUILD_DIR=".pio/build/MODALAI_M0139_via_UART"
FW_VER="ModalAI_M0139-3.3.2.$VERSION.bin"
TARGET="MODALAI_M0139_via_UART"

# BUILD_DIR=".pio/build/MODALAI_Frsky_RX_R9MM_R9MINI_via_UART"
# FW_VER="FrSky_R9Mini-3.2.1.$VERSION.bin"
# TARGET="MODALAI_Frsky_RX_R9MM_R9MINI_via_UART"

# Build application
platformio run --environment $TARGET # -v > build_output.txt

md5sum $BUILD_DIR/firmware.bin

if [ "$ENCRYPT" -eq 1 ]; then 
    # Encrypt application 
    echo "Encrpyting application"
    stm32-encrypt $BUILD_DIR/firmware.bin $BUILD_DIR/$FW_VER 0E9B89FC1D97C8B7DC4B2EA4DAB2B3FF02153BCF1ED07E8617E3EFF59BB26806
else
    echo "Leaving application un-encrypted"
    cp $BUILD_DIR/firmware.bin $BUILD_DIR/$FW_VER
fi

# Print out md5sum so we can verify what we push onto target matches what we just built and ecnrypted here
md5sum $BUILD_DIR/$FW_VER

# Copy/Push ELRS FW bin to desired location
# adb push $BUILD_DIR/$FW_VER /usr/share/modalai/voxl-elrs/firmware/rx

cp $BUILD_DIR/$FW_VER .
