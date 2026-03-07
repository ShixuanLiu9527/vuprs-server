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
SERVER_NAME="./server"
SHARED_LIB_DIR="./fftw3"

# Make sure only root user can use this script.

if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root.${NC}"
    exit 1
fi

# Check if ./xdma.ko, ./vuprs_server and ./fftw3 exist.

if [ ! -f "${XDMA_DRIVER_NAME}" ]; then
    echo -e "${RED}xdma driver: ${XDMA_DRIVER_NAME} not found.${NC}"
    exit 1
fi

if [ ! -f "${SERVER_NAME}" ]; then
    echo -e "${RED}server: ${SERVER_NAME} not found.${NC}"
    exit 1
fi

if [ ! -d "${SHARED_LIB_DIR}" ]; then
    echo -e "${RED}support shared dir: ${SHARED_LIB_DIR} not found.${NC}"
    exit 1
fi

for config_file in "${SERVER_CONFIG}" "${FPGA_CONFIG}" "${ARRAY_CONFIG}" "${FIR_CONFIG}"; do
    if [ ! -f "${config_file}" ]; then
        echo -e "${RED}Config file not found: ${config_file}${NC}"
        exit 1
    fi
done

# -------------------------- Load xdma driver -------------------------------

# Remove the existing xdma kernel module

lsmod | grep xdma  # List module and find xdma

if [ $? -eq 0 ]; then
    rmmod xdma
fi
echo -e "Loading xdma driver... "

# Load the driver in the default or interrupt drive mode.

insmod ./xdma.ko

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Kernel module did not load properly.${NC}"
    echo -e "${RED}FAILED${NC}"
    exit 1
fi

# Check if xdma has been successfully loaded.

cat /proc/devices | grep xdma > /dev/null

returnVal=$?
if [ $returnVal == 0 ]; then
    echo -e "The Kernel module installed correctly and the xmda devices were recognized."
else
    echo -e "${RED}Error: The Kernel module installed correctly, but no devices were recognized.${NC}"
    echo -e "${RED}FAILED${NC}"
    exit 1
fi

echo -e "${GREEN}DONE${NC}"

sleep 1

# ----------------------- Change environment path --------------------------

export LD_LIBRARY_PATH="${SHARED_LIB_DIR}":$LD_LIBRARY_PATH

# ----------------------- Start server -------------------------------------

echo -e "--- Start Server ---"
echo -e ""
echo -e "server: ${BLUE}${SERVER_NAME}${NC}"
echo -e "config files:"
echo -e "  Server: ${BLUE}${SERVER_CONFIG}${NC}"
echo -e "  FPGA: ${BLUE}${FPGA_CONFIG}${NC}"
echo -e "  Beam Forming array: ${BLUE}${ARRAY_CONFIG}${NC}"
echo -e "  FIR Filter Bank: ${BLUE}${FIR_CONFIG}${NC}"
echo -e ""

sleep 1

exec "${SERVER_NAME}" \
    --server-config "${SERVER_CONFIG}" \
    --fpga-config "${FPGA_CONFIG}" \
    --array-config "${ARRAY_CONFIG}" \
    --fir-config "${FIR_CONFIG}"
