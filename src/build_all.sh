#!/bin/bash

MODALAI_VERSION="" #don't set so build.sh will use git to pull modalai version
TARGETS_LIST=("m0184_rx" "m0193_rx" "m0193_tx" "BETAFPV_900_RX")
FACTORY_LIST=("m0184_rx" "m0193_rx" "m0193_tx")
FACTORY=0


print_usage () {
	echo ""
	echo "Build script for building ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encryption of firmware (pass in the key)"
    echo -e "   v) Version of firmware being built"
    echo -e "   --factory Build factory images for all ModalAI STM32 targets"
}


while [ $# -gt 0 ]; do
    case "$1" in
        "-h"|"--help")
            print_usage
            exit 0
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
        "--factory")
            FACTORY=1
            TARGETS_LIST=("${FACTORY_LIST[@]}")
            echo "Building factory images"
            shift
            ;;
        *)
            echo "invalid option $1"
            print_usage
            exit 1
            ;;
    esac
done

for element in "${TARGETS_LIST[@]}"; do
    TARGET_CMD="-t ${element}"
    if [ "$FACTORY" -eq 1 ]; then
        TARGET_CMD="$TARGET_CMD --factory"
    fi
    echo "target command: $TARGET_CMD"

    bash build.sh $TARGET_CMD
done