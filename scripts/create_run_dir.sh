#!/bin/bash

RED='\033[1;31m'
GREEN='\033[1;32m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
VUPRS_ROOT="${SCRIPT_DIR}/.."
BUILD_OUTPUT="${VUPRS_ROOT}/build"

XDMA_DRIVER="${BUILD_OUTPUT}/xdma/xdma.ko"
SERVER="${BUILD_OUTPUT}/server"
CONFIGS="${VUPRS_ROOT}/configs"
FFTW_OUTPUT_DIR="${BUILD_OUTPUT}/3rdparty/fftw3"
RUN_SHELL="${SCRIPT_DIR}/run_server.sh"
RKNPU2_LIB_DIR="${VUPRS_ROOT}/3rdparty/rknpu2/librknn_api/aarch64"

# Check file or directories exits.

for file in "${XDMA_DRIVER}" "${SERVER}" "${RUN_SHELL}"; do
    if [ ! -f "${file}" ]; then
        echo -e "${RED}File not found: ${file}${NC}"
        exit 1
    fi
done
for dir in "${FFTW_OUTPUT_DIR}" "${CONFIGS}" "${RKNPU2_LIB_DIR}"; do
    if [ ! -d "${dir}" ]; then
        echo -e "${RED}Dir not found: ${dir}${NC}"
        exit 1
    fi
done

# Copy file

OUTPUT_DIR="${VUPRS_ROOT}/run_dir/"

mkdir -p "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}/fftw3"
mkdir -p "${OUTPUT_DIR}/configs"
mkdir -p "${OUTPUT_DIR}/rknpu2"
mkdir -p "${OUTPUT_DIR}/model"

cp "${XDMA_DRIVER}" "${OUTPUT_DIR}/xdma.ko"
cp "${RUN_SHELL}" "${OUTPUT_DIR}/run_server.sh"
cp "${SERVER}" "${OUTPUT_DIR}/server"
cp "${RKNPU2_LIB_DIR}"/*.so "${OUTPUT_DIR}/rknpu2/"
cp "${CONFIGS}/"* "${OUTPUT_DIR}/configs/"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so" "${OUTPUT_DIR}/fftw3/"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so.3" "${OUTPUT_DIR}/fftw3/"
cp "${FFTW_OUTPUT_DIR}/libfftw3_threads.so.3.6.9" "${OUTPUT_DIR}/fftw3/"
cp "${FFTW_OUTPUT_DIR}/libfftw3.so" "${OUTPUT_DIR}/fftw3/"
cp "${FFTW_OUTPUT_DIR}/libfftw3.so.3" "${OUTPUT_DIR}/fftw3/"
cp "${FFTW_OUTPUT_DIR}/libfftw3.so.3.6.9" "${OUTPUT_DIR}/fftw3/"

echo -e "${GREEN}Generate DONE${NC}"
echo -e "${GREEN}You can find run_dir in ./run_dir${NC}"
echo -e "${BLUE}Note: Before running, you need to place the *.rknn in the directory: run_dir/model ${NC}"
