#!/bin/bash

# ---------------------------------- XDMA MAKE Config --------------------------------------------

XDMA__ARCH="arm64"
XDMA__CROSS_COMPILE="/home/lsx/source/linux/rk356x_linux/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-"
XDMA__KERNEL_DIR="/home/lsx/source/linux/rk356x_linux/kernel"

# ------------------------------------------------------------------------------------------------

RED='\033[31m'
GREEN='\033[32m'
BLUE='\033[34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"  # Current dir
BUILD_OUTPUT="${SCRIPT_DIR}/build"  # Build dir

XDMA_SOURCE_DIR="${SCRIPT_DIR}/xdma_driver/xdma"
XDMA_OUTPUT_DIR="${SCRIPT_DIR}/build/xdma"

build_xdma()
{
    echo ""
    echo "${BLUE}XDMA driver setting:${NC}"
    echo "  ARCH: ${GREEN}${XDMA__ARCH}${NC}"
    echo "  CROSS_COMPILE: ${GREEN}${XDMA__CROSS_COMPILE}${NC}"
    echo "  Kernel dir: ${GREEN}${XDMA__KERNEL_DIR}${NC}"
    echo ""

    echo "${BLUE}Build XDMA Driver...${NC}"
    cd "${XDMA_SOURCE_DIR}" || exit 1
    echo "${BLUE}Make all...${NC}"
    make all ARCH="${XDMA__ARCH}" CROSS_COMPILE="${XDMA__CROSS_COMPILE}" BUILDSYSTEM_DIR="${XDMA__KERNEL_DIR}"

    if [ ! -f "${XDMA_SOURCE_DIR}/xdma.ko" ]; then
        echo "${RED}file not found: ${file}${NC}"
        echo "${RED}Build XDMA Driver FAILED${NC}"
        exit 1
    fi

    cp "${XDMA_SOURCE_DIR}/xdma.ko" "${XDMA_OUTPUT_DIR}/xdma.ko" 2>/dev/null || echo "xdma.ko not found"
    echo "${GREEN}Build XDMA Driver DONE${NC}"
}

build_server()
{
    echo "${BLUE}Build Server & Tool...${NC}"
    cd "${BUILD_OUTPUT}" || exit 1
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../rk3568_toolchain.cmake
    make
    echo "${GREEN}Build Server & Tool DONE${NC}"
}

clean_xdma()
{
    echo "${BLUE}Cleaning...${NC}"
    cd "${XDMA_SOURCE_DIR}" || exit 1
    make clean ARCH="${XDMA__ARCH}" CROSS_COMPILE="${XDMA__CROSS_COMPILE}" BUILDSYSTEM_DIR="${XDMA__KERNEL_DIR}"
}

clean_server()
{
    cd "${SCRIPT_DIR}"
    rm -rf "${BUILD_OUTPUT}"
}

show_help()
{
    echo ""
    echo "Usage $0 [OPTION]"
    echo "options:"
    echo "  all          - compile XDMA Driver, Server and FPGA-Tool"
    echo "  xdma         - compile XDMA Driver"
    echo "  server       - compile XDMA Driver, Server and FPGA-Tool"
    echo "  clean-all    - clean all complied files"
    echo "  clean-xdma   - clean XDMA Driver complied files"
    echo "  clean-server - clean Server and FPGA-Tool complied files"
    echo "  help         - show help"
}

mkdir -p "${BUILD_OUTPUT}"
mkdir -p "${XDMA_OUTPUT_DIR}"

case "$1" in
    all)
        build_xdma
        build_server
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    xdma)
        build_xdma
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    server)
        build_server
        echo "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    clean-all)
        clean_xdma
        clean_server
        echo "${GREEN}Clean DONE${NC}"
        ;;

    clean-xdma)
        clean_xdma
        echo "${GREEN}Clean DONE${NC}"
        ;;

    clean-server)
        clean_server
        echo "${GREEN}Clean DONE${NC}"
        ;;
    
    *)
        show_help
        ;;
esac

cd "${SCRIPT_DIR}"
echo ""
