#!/bin/bash
XDMA_DRIVER_MODULE=./xdma.ko
# Remove the existing xdma kernel module
RED='\033[1;31m'
GREEN='\033[1;32m'
BLUE='\033[1;34m'
NC='\033[0m'
lsmod | grep xdma  # List module and find xdma
if [ $? -eq 0 ]; then
    rmmod xdma
fi
echo -e "Loading xdma driver... "
# Load the driver in the default or interrupt drive mode.
insmod "${XDMA_DRIVER_MODULE}"
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
