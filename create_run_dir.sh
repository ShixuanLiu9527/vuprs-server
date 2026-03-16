#!/bin/bash

RED='\033[31m'
GREEN='\033[32m'
BLUE='\033[34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_OUTPUT="${SCRIPT_DIR}/build"

XDMA_DRIVER="${BUILD_OUTPUT}/xdma/xdma.ko"
SERVER="${BUILD_OUTPUT}/server"
RUN_SHELL="${SCRIPT_DIR}/run_server.sh"

FFTW_OUTPUT_DIR="${BUILD_OUTPUT}/fftw3"

# Check file or directories exits.

for file in "${XDMA_DRIVER}" "${SERVER}" "${RUN_SHELL}"; do
    if [ ! -f "${file}" ]; then
        echo "${RED}Config file not found: ${file}${NC}"
        exit 1
    fi
done
if [ ! -d "${FFTW_OUTPUT_DIR}" ]; then
    echo "${RED}dir: ${FFTW_OUTPUT_DIR} not found.${NC}"
    exit 1
fi

# Copy file

OUTPUT_DIR="${SCRIPT_DIR}/run_dir/"

mkdir -p "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}/fftw3"
mkdir -p "${OUTPUT_DIR}/configs"

cp "${XDMA_DRIVER}" "${OUTPUT_DIR}/xdma.ko"
cp "${RUN_SHELL}" "${OUTPUT_DIR}/run_server.sh"
cp "${SERVER}" "${OUTPUT_DIR}/server"

cp "${FFTW_OUTPUT_DIR}/libfftw3.so" "${OUTPUT_DIR}/fftw3/libfftw3.so"
cp "${FFTW_OUTPUT_DIR}/libfftw3.so.3" "${OUTPUT_DIR}/fftw3/libfftw3.so.3"
cp "${FFTW_OUTPUT_DIR}/libfftw3.so.3.6.9" "${OUTPUT_DIR}/fftw3/libfftw3.so.3.6.9"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so" "${OUTPUT_DIR}/fftw3/libfftw3_threads.so"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so.3" "${OUTPUT_DIR}/fftw3/libfftw3_threads.so.3"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so.3.6.9" "${OUTPUT_DIR}/fftw3/libfftw3_threads.so.3.6.9"

echo "${GREEN}Generate DONE${NC}"
echo "${GREEN}Yon can find run_dir in ./${NC}"