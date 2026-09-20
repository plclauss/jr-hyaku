#!/bin/bash

set -e

RED='\e[31m'
GREEN='\e[32m'
NC='\e[0m'

print_usage() {
    echo -e "${NC}Usage: ./build.sh [-l] [-d] [-h]${NC}"
    echo -e "${NC}\t-l: Compile with logs enabled.${NC}"
    echo -e "${NC}\t-d: Compile with DEBUG statements.${NC}"
    echo -e "${NC}\t-h: Print help/usage statement.${NC}"
}

LOGGER_ENABLE=0
DEBUG=0
while getopts "ldh" opt; do
    case $opt in
        l)
            LOGGER_ENABLE=1
            ;;
        d)
            DEBUG=1
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

CMAKE_DEFINES=()
if [[ $LOGGER_ENABLE -eq 1 ]]; then
    CMAKE_DEFINES+=("-DLOGGER_ENABLE=1")
else
    CMAKE_DEFINES+=("-ULOGGER_ENABLE")
fi

if [[ $DEBUG -eq 1 ]]; then
    CMAKE_DEFINES+=("-DDEBUG=1")
else
    CMAKE_DEFINES+=("-UDEBUG")
fi

cmake "${CMAKE_DEFINES[@]}" -DCMAKE_TOOLCHAIN_FILE=../aarch64-toolchain.cmake .. && cmake --build .
exit 0
