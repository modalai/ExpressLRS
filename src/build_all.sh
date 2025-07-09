#!/bin/bash

MODALAI_VERSION="" #don't set so build.sh will use git to pull modalai version
TARGETS_LIST=("m0184_rx" "m0193_rx" "m0193_tx" "BETAFPV_900_RX")


print_usage () {
	echo ""
	echo "Build script for building ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encryption of firmware (pass in the key)"
    echo -e "   v) Version of firmware being built"
}


while getopts "e:v:" opt; do
    case $opt in
        "h")
            print_usage
            exit 0
            ;;
        "v")
            MODALAI_VERSION=${OPTARG}
            echo "Using version #: $MODALAI_VERSION"
            ;;
        *)
            echo "invalid option $arg"
            print_usage
            exit 1
            ;;
    esac
done

for element in "${TARGETS_LIST[@]}"; do
    # TARGET_CMD="-v ${MODALAI_VERSION} -t ${element}"
    TARGET_CMD="-t ${element}"
    echo "target command: $TARGET_CMD"

    bash build.sh $TARGET_CMD
done