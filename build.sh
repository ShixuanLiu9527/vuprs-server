#!/bin/bash

RED='\033[31m'
GREEN='\033[32m'
BLUE='\033[34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_OUTPUT="${SCRIPT_DIR}/build"

XDMA_SOURCE_DIR="${SCRIPT_DIR}/xdma_driver/xdma"
XDMA_OUTPUT_DIR="${SCRIPT_DIR}/build/xdma"

mkdir -p "${BUILD_OUTPUT}"
mkdir -p "${XDMA_OUTPUT_DIR}"

case "$1" in
    all)
        # Build XDMA driver

        echo "${BLUE}Build XDMA Driver...${NC}"
        cd "${XDMA_SOURCE_DIR}" || exit 1
        echo "${BLUE}Make all...${NC}"
        make all
        cp "${XDMA_SOURCE_DIR}/xdma.ko" "${XDMA_OUTPUT_DIR}/xdma.ko" 2>/dev/null || echo "xdma.ko not found"
        echo "${GREEN}Build XDMA Driver DONE${NC}"
        
        # Build Server & Tool

        echo "${BLUE}Build Server & Tool...${NC}"
        cd "${BUILD_OUTPUT}" || exit 1
        cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
        make
        echo "${GREEN}Build Server & Tool DONE${NC}"
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    xdma)
        # Build XDMA driver only

        echo "${BLUE}Build XDMA Driver...${NC}"
        cd "${XDMA_SOURCE_DIR}" || exit 1
        echo "${BLUE}Make all...${NC}"
        make all
        cp "${XDMA_SOURCE_DIR}/xdma.ko" "${XDMA_OUTPUT_DIR}/xdma.ko" 2>/dev/null || echo "xdma.ko not found"
        echo "${GREEN}Build XDMA Driver DONE${NC}"
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    server)
        # Build Server & Tool only

        echo "${BLUE}Build Server & Tool...${NC}"
        cd "${BUILD_OUTPUT}" || exit 1
        cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
        make
        echo "${GREEN}Build Server & Tool DONE${NC}"
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    clean)
        # Clean everything

        echo "${BLUE}Cleaning...${NC}"
        cd "${XDMA_SOURCE_DIR}" || exit 1
        make clean
        cd "${SCRIPT_DIR}"
        rm -rf "${BUILD_OUTPUT}"
        echo "${GREEN}Clean DONE${NC}"
        ;;
    
    *)
        # Help

        echo ""
        echo "Usage $0 [OPTION]"
        echo "options:"
        echo "  all      - compile XDMA Driver, Server and FPGA-Tool"
        echo "  xdma     - compile XDMA Driver"
        echo "  server   - compile XDMA Driver, Server and FPGA-Tool"
        echo "  clean    - clean all complied files"
        echo "  help     - show help"
        ;;
esac

cd "${SCRIPT_DIR}"
echo ""
