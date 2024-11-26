#!/bin/bash

MODALAI_VERSION="0"
ENCRYPT=0
ENCRYPT_KEY=""
TARGETS_LIST=("m0184_rx" "m0184_tx" "m0184_hwil_tx" "m0184_hwil_rx" "m0193_rx" "m0193_tx" "m0193_hwil_tx" "m0193_hwil_rx")


print_usage () {
	echo ""
	echo "Build script for buidling ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encrpytion of firmware (pass in the key)"
    echo -e "   v) Version of firmware being built"
}


while getopts "e:v:" opt; do
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
        *)
            echo "invalid option $arg"
            print_usage
            exit 1
            ;;
    esac
done

for element in "${TARGETS_LIST[@]}"; do
    TARGET_CMD=""
    if [[ ENCRYPT ]] then
        TARGET_CMD="-e ${ENCRYPT_KEY} -v ${MODALAI_VERSION} -t ${element}"
    else
        TARGET_CMD="-v ${MODALAI_VERSION} -t ${element}"
    fi

    bash build.sh $TARGET_CMD
done