#!/bin/bash

# ------------------------------------ Config files -------------------------

FPGA_CONFIG="./fpga_config.json"
SERVER_CONFIG="./server_config.json"
ARRAY_CONFIG="./array_config.json"
FIR_CONFIG="./fir_config.json"

# --------------------------- Security Check --------------------------------

RED='\033[31m'
GREEN='\033[32m'
BLUE='\033[34m'
NC='\033[0m'  # No Color

XDMA_DRIVER_NAME="./xdma.ko"
SERVER_NAME="./vuprs_server"
SHARED_LIB_DIR="./fftw3"

# Make sure only root user can use this script.

if [[ $EUID -ne 0 ]]; then
    echo "${RED}This script must be run as root.${NC}"
    exit 1
fi

# Check if ./xdma.ko, ./vuprs_server and ./fftw3 exist.

if [ ! -f "${XDMA_DRIVER_NAME}" ]; then
    echo "${RED}xdma driver: ${XDMA_DRIVER_NAME} not found.${NC}"
    exit 1
fi

if [ ! -f "${SERVER_NAME}" ]; then
    echo "${RED}server: ${SERVER_NAME} not found.${NC}"
    exit 1
fi

if [ ! -d "${SHARED_LIB_DIR}" ]; then
    echo "${RED}support shared dir: ${SHARED_LIB_DIR} not found.${NC}"
    exit 1
fi

for config_file in "${SERVER_CONFIG}" "${FPGA_CONFIG}" "${ARRAY_CONFIG}" "${FIR_CONFIG}"; do
    if [ ! -f "${config_file}" ]; then
        echo "${RED}Config file not found: ${config_file}${NC}"
        exit 1
    fi
done

# -------------------------- Load xdma driver -------------------------------

# Remove the existing xdma kernel module

lsmod | grep xdma  # List module and find xdma

if [ $? -eq 0 ]; then
    rmmod xdma
fi
echo "Loading xdma driver... "

# Load the driver in the default or interrupt drive mode.

insmod ./xdma.ko

if [ $? -ne 0 ]; then
    echo "${RED}Error: Kernel module did not load properly.${NC}"
    echo "${RED}FAILED${NC}"
    exit 1
fi

# Check if xdma has been successfully loaded.

cat /proc/devices | grep xdma > /dev/null

returnVal=$?
if [ $returnVal == 0 ]; then
    echo "The Kernel module installed correctly and the xmda devices were recognized."
else
    echo "${RED}Error: The Kernel module installed correctly, but no devices were recognized.${NC}"
    echo "${RED}FAILED${NC}"
    exit 1
fi

echo "${GREEN}DONE${NC}"

# ----------------------- Change environment path --------------------------

export LD_LIBRARY_PATH="${SHARED_LIB_DIR}":$LD_LIBRARY_PATH

# ----------------------- Start server -------------------------------------

echo "--- Start Server ---"
echo ""
echo "server: ${BLUE}${SERVER_NAME}${NC}"
echo "config files:"
echo "  Server: ${BLUE}${SERVER_CONFIG}${NC}"
echo "  FPGA: ${BLUE}${FPGA_CONFIG}${NC}"
echo "  Beam Forming array: ${BLUE}${ARRAY_CONFIG}${NC}"
echo "  FIR Filter Bank: ${BLUE}${FIR_CONFIG}${NC}"
echo ""

sleep 1

exec "${SERVER_NAME}" \
    --server-config "${SERVER_CONFIG}" \
    --fpga-config "${FPGA_CONFIG}" \
    --array-config "${ARRAY_CONFIG}" \
    --fir-config "${FIR_CONFIG}"
