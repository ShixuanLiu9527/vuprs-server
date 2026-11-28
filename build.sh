#!/bin/bash

# ---------------------------------- XDMA MAKE Config --------------------------------------------

XDMA__ARCH="arm64"
XDMA__CROSS_COMPILE="/home/lsx/source/linux/rk356x_linux/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-"
XDMA__KERNEL_DIR="/home/lsx/source/linux/rk356x_linux/kernel"

# ------------------------------------------------------------------------------------------------

RED='\033[1;31m'
GREEN='\033[1;32m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"  # Current dir
BUILD_OUTPUT="${SCRIPT_DIR}/build"  # Build dir

XDMA_SOURCE_DIR="${SCRIPT_DIR}/pcie/xdma"
XDMA_OUTPUT_DIR="${SCRIPT_DIR}/build/xdma"
BOARD_SCRIPTS_OUTPUT="${BUILD_OUTPUT}/scripts"

build_xdma()
{
    echo ""
    echo -e "${BLUE}XDMA driver setting:${NC}"
    echo -e "  ARCH: ${GREEN}${XDMA__ARCH}${NC}"
    echo -e "  CROSS_COMPILE: ${GREEN}${XDMA__CROSS_COMPILE}${NC}"
    echo -e "  Kernel dir: ${GREEN}${XDMA__KERNEL_DIR}${NC}"
    echo ""

    echo -e "${BLUE}Build XDMA Driver...${NC}"
    cd "${XDMA_SOURCE_DIR}" || exit 1
    echo -e "${BLUE}Make all...${NC}"
    make all ARCH="${XDMA__ARCH}" CROSS_COMPILE="${XDMA__CROSS_COMPILE}" BUILDSYSTEM_DIR="${XDMA__KERNEL_DIR}"

    if [ ! -f "${XDMA_SOURCE_DIR}/xdma.ko" ]; then
        echo -e "${RED}file not found: ${XDMA_SOURCE_DIR}/xdma.ko${NC}"
        echo -e "${RED}Build XDMA Driver FAILED${NC}"
        exit 1
    fi

    # 将构建产物统一移动到 build/xdma/ 下,保持源码目录干净
    mv -f xdma.ko "${XDMA_OUTPUT_DIR}/xdma.ko" || exit 1
    mv -f *.o *.mod *.mod.c *.mod.o .*.cmd modules.order Module.symvers "${XDMA_OUTPUT_DIR}/" 2>/dev/null
    rm -rf "${XDMA_OUTPUT_DIR}/.tmp_versions"
    mv -f .tmp_versions "${XDMA_OUTPUT_DIR}/" 2>/dev/null
    echo -e "${GREEN}Build XDMA Driver DONE${NC}"
}

build_server()
{
    echo -e "${BLUE}Build Server & Tool...${NC}"
    cd "${BUILD_OUTPUT}" || exit 1
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../cmake/rk3568_toolchain.cmake
    make
    echo -e "${GREEN}Build Server & Tool DONE${NC}"
}

copy_board_scripts()
{
    echo -e "${BLUE}Copy board scripts to ${BOARD_SCRIPTS_OUTPUT}...${NC}"
    mkdir -p "${BOARD_SCRIPTS_OUTPUT}"
    for script in \
        net_config.sh \
        setup_dhcp.sh \
        install_server_service.sh \
        setup_all.sh \
        mount_xdma.sh \
        run_server.sh; do
        cp -f "${SCRIPT_DIR}/scripts/${script}" "${BOARD_SCRIPTS_OUTPUT}/${script}" || exit 1
    done
    chmod +x "${BOARD_SCRIPTS_OUTPUT}"/*.sh
    echo -e "${GREEN}Copy board scripts DONE${NC}"
}

clean_xdma()
{
    echo -e "${BLUE}Cleaning...${NC}"
    cd "${XDMA_SOURCE_DIR}" || exit 1
    make clean ARCH="${XDMA__ARCH}" CROSS_COMPILE="${XDMA__CROSS_COMPILE}" BUILDSYSTEM_DIR="${XDMA__KERNEL_DIR}"
    rm -rf "${XDMA_OUTPUT_DIR}"
}

clean_server()
{
    cd "${SCRIPT_DIR}"
    rm -rf "${BUILD_OUTPUT}"
}

deploy_run_dir()
{
    local RUN_DIR_SCRIPT="${SCRIPT_DIR}/scripts/create_run_dir.sh"
    # Only deploy when all build outputs exist, otherwise remind the user.
    if [ -f "${BUILD_OUTPUT}/xdma/xdma.ko" ] && [ -f "${BUILD_OUTPUT}/server" ] && [ -f "${BUILD_OUTPUT}/fpga/cli/fpga_tool" ]; then
        bash "${RUN_DIR_SCRIPT}" || exit 1
    else
        echo -e "${BLUE}Note: skip generating vuprs, missing build outputs (build/xdma/xdma.ko, build/server or build/fpga/cli/fpga_tool).${NC}"
    fi
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
mkdir -p "${BOARD_SCRIPTS_OUTPUT}"

case "$1" in
    all)
        build_xdma
        build_server
        deploy_run_dir
        copy_board_scripts
        echo -e "${GREEN}You can find the output in ./build${NC}"
        ;;

    xdma)
        build_xdma
        copy_board_scripts
        echo -e "${GREEN}You can find the output in ./build${NC}"
        ;;

    server)
        build_server
        deploy_run_dir
        copy_board_scripts
        echo -e "${GREEN}You can find the output in ./build${NC}"
        ;;
    
    clean-all)
        clean_xdma
        clean_server
        echo -e "${GREEN}Clean DONE${NC}"
        ;;

    clean-xdma)
        clean_xdma
        echo -e "${GREEN}Clean DONE${NC}"
        ;;

    clean-server)
        clean_server
        echo -e "${GREEN}Clean DONE${NC}"
        ;;
    
    *)
        show_help
        ;;
esac

cd "${SCRIPT_DIR}"
echo ""
