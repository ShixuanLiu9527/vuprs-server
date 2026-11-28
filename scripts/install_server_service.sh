#!/bin/bash

# Usage:
#   sudo sh ./install_server_service.sh [vuprs_dir] [--start]
#
# vuprs_dir defaults to the current directory when it contains run_server.sh
# and server, otherwise to ./vuprs.

RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

SERVICE_FILE="/etc/systemd/system/vuprs-server.service"
START_NOW=0
VUPRS_DIR=""

for arg in "$@"; do
    case "${arg}" in
        --start) START_NOW=1 ;;
        *) VUPRS_DIR="${arg}" ;;
    esac
done

if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root.${NC}"
    exit 1
fi

if [ -z "${VUPRS_DIR}" ]; then
    if [ -f "./run_server.sh" ] && [ -f "./server" ]; then
        VUPRS_DIR="$(pwd)"
    elif [ -f "./vuprs/run_server.sh" ] && [ -f "./vuprs/server" ]; then
        VUPRS_DIR="$(pwd)/vuprs"
    else
        echo -e "${RED}Cannot find the vuprs runtime directory.${NC}"
        echo -e "${YELLOW}Usage: sudo sh ./install_server_service.sh /path/to/vuprs [--start]${NC}"
        exit 1
    fi
fi

VUPRS_DIR="$(cd "${VUPRS_DIR}" && pwd)" || {
    echo -e "${RED}Invalid vuprs directory.${NC}"
    exit 1
}

for file in run_server.sh server xdma.ko; do
    if [ ! -f "${VUPRS_DIR}/${file}" ]; then
        echo -e "${RED}Missing ${VUPRS_DIR}/${file}${NC}"
        exit 1
    fi
done
for dir in configs fftw3 rknpu2; do
    if [ ! -d "${VUPRS_DIR}/${dir}" ]; then
        echo -e "${RED}Missing ${VUPRS_DIR}/${dir}${NC}"
        exit 1
    fi
done

if ! command -v systemctl >/dev/null 2>&1; then
    echo -e "${RED}systemctl not found; cannot install a systemd service.${NC}"
    echo -e "${YELLOW}Start the server manually with: cd ${VUPRS_DIR} && sudo sh ./run_server.sh${NC}"
    exit 1
fi

cat > "${SERVICE_FILE}" <<EOF
[Unit]
Description=VUPRS server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=${VUPRS_DIR}
ExecStart=/bin/bash "${VUPRS_DIR}/run_server.sh"
Restart=on-failure
RestartSec=2

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable vuprs-server.service

echo -e "${GREEN}Installed ${SERVICE_FILE}${NC}"
echo -e "  WorkingDirectory: ${BLUE}${VUPRS_DIR}${NC}"

if [ "${START_NOW}" -eq 1 ]; then
    if systemctl restart vuprs-server.service; then
        sleep 1
        if systemctl is-active --quiet vuprs-server.service; then
            echo -e "${GREEN}vuprs-server.service is running.${NC}"
        else
            echo -e "${RED}Service failed to start. Check: journalctl -u vuprs-server -n 50${NC}"
            exit 1
        fi
    else
        echo -e "${RED}Failed to start vuprs-server.service.${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}Service enabled at boot. Start it now with:${NC}"
    echo -e "  sudo systemctl start vuprs-server.service"
fi
