#!/bin/bash

RED='\033[1;31m'
GREEN='\033[1;32m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

# insmod/rmmod require root privileges.
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root.${NC}"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
CURRENT_DIR="$(pwd)"

# Look for xdma.ko next to this script, in build/xdma/, or in the current directory.
XDMA_DRIVER_MODULE=""
for candidate in \
    "${SCRIPT_DIR}/xdma.ko" \
    "${SCRIPT_DIR}/xdma/xdma.ko" \
    "${SCRIPT_DIR}/build/xdma/xdma.ko" \
    "${CURRENT_DIR}/xdma.ko" \
    "${CURRENT_DIR}/xdma/xdma.ko" \
    "${CURRENT_DIR}/build/xdma/xdma.ko"; do
    if [ -f "${candidate}" ]; then
        XDMA_DRIVER_MODULE="${candidate}"
        break
    fi
done

if [ -z "${XDMA_DRIVER_MODULE}" ]; then
    echo -e "${RED}xdma.ko not found.${NC}"
    echo -e "${RED}Place xdma.ko next to this script, in build/xdma/, or in the current directory.${NC}"
    exit 1
fi

echo -e "${BLUE}Using xdma driver: ${XDMA_DRIVER_MODULE}${NC}"

# Remove the existing xdma kernel module.
if lsmod | grep -q xdma; then
    echo -e "${BLUE}Removing existing xdma module...${NC}"
    if ! rmmod xdma; then
        echo -e "${RED}Failed to remove the existing xdma module (it may be in use).${NC}"
        exit 1
    fi
fi

echo -e "${BLUE}Loading xdma driver...${NC}"
if ! insmod "${XDMA_DRIVER_MODULE}"; then
    echo -e "${RED}Error: Kernel module did not load properly.${NC}"
    echo -e "${RED}FAILED${NC}"
    exit 1
fi

# Check whether xdma has been successfully loaded.
if grep -q xdma /proc/devices; then
    echo -e "${GREEN}The kernel module installed correctly and the xdma devices were recognized.${NC}"
else
    echo -e "${RED}Error: The kernel module installed correctly, but no devices were recognized.${NC}"
    echo -e "${RED}FAILED${NC}"
    exit 1
fi

echo -e "${GREEN}DONE${NC}"
