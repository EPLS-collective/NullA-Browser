#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

BUILD_TYPE="Release"
ACTION="${1:-run}"

show_help() {
    echo -e "${GREEN}NullA Browser - Build Tool${NC}"
    echo -e "Usage: ./run.sh [option]\n"
    echo -e "${YELLOW}Options:${NC}"
    echo "  run      - Build in Release mode and run (default)"
    echo "  debug    - Build in Debug mode and run"
    echo "  release  - Build in Release mode only"
    echo "  clean    - Remove build directory"
    echo "  rebuild  - Clean, build in Release mode, and run"
    echo "  help / -h  - Display this help message"
}

if [[ "$ACTION" == "help" || "$ACTION" == "-h" ]]; then
    show_help
    exit 0
fi

if [[ "$ACTION" == "clean" || "$ACTION" == "rebuild" ]]; then
    echo -e "${RED}➜ Cleaning build directory...${NC}"
    rm -rf build
    [[ "$ACTION" == "clean" ]] && exit 0
fi

mkdir -p build && cd build || exit 1

if [[ "$ACTION" == "debug" ]]; then
    BUILD_TYPE="Debug"
fi

echo -e "${GREEN}➜ Building (${BUILD_TYPE})...${NC}"
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .. || exit 1
cmake --build . -j$(nproc) || exit 1

cd ..

if [[ "$ACTION" == "run" || "$ACTION" == "rebuild" || "$ACTION" == "debug" ]]; then
    echo -e "${GREEN}➜ Launching application...${NC}"
    if [ -f "build/bin/NullA" ]; then
        ./build/bin/NullA
    elif [ -f "build/NullA" ]; then
        ./build/NullA
    else
        echo -e "${RED}✗ Executable not found!${NC}"
    fi
fi
