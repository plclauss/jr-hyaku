#!/bin/bash

RED='\e[31m'
GREEN='\e[32m'
NC='\e[0m'

print_usage() {
    echo -e "${NC}Usage: ./build.sh { <-l> | <-h> }${NC}"
    echo -e "${NC}\t-l: Compile for Linux systems.${NC}"
    echo -e "${NC}\t-h: Print help/usage statement.${NC}"
}

EXPECTED_ARGS=1
if [ "$#" -ne "$EXPECTED_ARGS" ]; then
    echo -e "${RED}Error: Incorrect number of arguments.${NC}"
    print_usage
    exit 1
fi

TARGET_PLATFORM=""
while getopts "lh" opt; do
    case $opt in
        l)
            TARGET_PLATFORM="LINUX"
            ;;
        h)
            print_usage
            exit 0
            ;;
        \?)
            print_usage
            exit 1
            ;;
    esac
done

if [ "$TARGET_PLATFORM" = "LINUX" ]; then
    cmake -DCMAKE_TOOLCHAIN_FILE=../aarch64-toolchain.cmake -DTARGET_PLATFORM=$TARGET_PLATFORM .. && cmake --build .
    exit 0
else
    print_usage
    exit 1
fi

exit 1 # Failsafe
