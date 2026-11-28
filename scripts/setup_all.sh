#!/bin/bash

# One-key board provisioning:
#   1. set the board Ethernet IP
#   2. install/start the dnsmasq DHCP service
#   3. install/start the VUPRS server service
#
# Usage:
#   sudo bash ./setup_all.sh [options]
#
# Options:
#   --iface <name>       Ethernet interface (default: eth0)
#   --ip <address>       Board static IPv4 address (default: 192.168.50.10)
#   --pool-start <addr>  DHCP pool start (default: <subnet>.100)
#   --pool-end <addr>    DHCP pool end   (default: <subnet>.200)
#   --no-start           Enable services at boot but do not start now
#   -h, --help           Show help
#
# Environment overrides: VUPRS_IFACE, VUPRS_IP, VUPRS_POOL_START,
#                        VUPRS_POOL_END, VUPRS_DIR

if [ -z "${BASH_VERSION:-}" ]; then
    exec /bin/bash "$0" "$@"
fi

set -u

RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
IFACE="${VUPRS_IFACE:-eth0}"
BOARD_IP="${VUPRS_IP:-192.168.50.10}"
NETMASK="255.255.255.0"
DO_START=1
POOL_START="${VUPRS_POOL_START:-}"
POOL_END="${VUPRS_POOL_END:-}"

usage()
{
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --iface) IFACE="$2"; shift 2 ;;
        --ip) BOARD_IP="$2"; shift 2 ;;
        --pool-start) POOL_START="$2"; shift 2 ;;
        --pool-end) POOL_END="$2"; shift 2 ;;
        --no-start) DO_START=0; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo -e "${RED}Unknown option: $1${NC}"; usage; exit 1 ;;
    esac
done

if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root.${NC}"
    exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
    echo -e "${RED}systemd is required (systemctl not found).${NC}"
    exit 1
fi

if ! command -v ip >/dev/null 2>&1; then
    echo -e "${RED}ip command not found.${NC}"
    exit 1
fi

if ! ip link show dev "${IFACE}" >/dev/null 2>&1; then
    echo -e "${RED}Interface ${IFACE} not found. Available interfaces:${NC}"
    ip -br link
    exit 1
fi

if [[ ! "${BOARD_IP}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
    echo -e "${RED}Invalid board IP: ${BOARD_IP}${NC}"
    exit 1
fi

SUBNET="${BOARD_IP%.*}"
[ -z "${POOL_START}" ] && POOL_START="${SUBNET}.100"
[ -z "${POOL_END}" ] && POOL_END="${SUBNET}.200"

if [[ ! "${POOL_START}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] ||
   [[ ! "${POOL_END}" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
    echo -e "${RED}Invalid DHCP pool range: ${POOL_START} - ${POOL_END}${NC}"
    exit 1
fi

if [ "${POOL_START%.*}" != "${SUBNET}" ] || [ "${POOL_END%.*}" != "${SUBNET}" ]; then
    echo -e "${RED}DHCP pool must be in ${SUBNET}.0/24${NC}"
    exit 1
fi

POOL_START_LAST="${POOL_START##*.}"
POOL_END_LAST="${POOL_END##*.}"
BOARD_LAST="${BOARD_IP##*.}"
if [ "${POOL_START_LAST}" -gt "${POOL_END_LAST}" ]; then
    echo -e "${RED}DHCP pool start is greater than end.${NC}"
    exit 1
fi
if [ "${BOARD_LAST}" -ge "${POOL_START_LAST}" ] && [ "${BOARD_LAST}" -le "${POOL_END_LAST}" ]; then
    echo -e "${RED}Board IP ${BOARD_IP} must not be inside the DHCP pool.${NC}"
    exit 1
fi

if [ -z "${VUPRS_DIR:-}" ]; then
    if [ -f "${SCRIPT_DIR}/run_server.sh" ] && [ -f "${SCRIPT_DIR}/server" ]; then
        VUPRS_DIR="${SCRIPT_DIR}"
    elif [ -f "${SCRIPT_DIR}/../vuprs/run_server.sh" ]; then
        VUPRS_DIR="${SCRIPT_DIR}/../vuprs"
    elif [ -f "$(pwd)/run_server.sh" ] && [ -f "$(pwd)/server" ]; then
        VUPRS_DIR="$(pwd)"
    else
        echo -e "${RED}Cannot find the vuprs runtime directory.${NC}"
        echo -e "${YELLOW}Run this script from inside vuprs/, or set VUPRS_DIR=/path/to/vuprs.${NC}"
        exit 1
    fi
fi
VUPRS_DIR="$(cd "${VUPRS_DIR}" && pwd)"

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
for helper in net_config.sh setup_dhcp.sh install_server_service.sh; do
    if [ ! -f "${SCRIPT_DIR}/${helper}" ]; then
        echo -e "${RED}Missing helper script: ${SCRIPT_DIR}/${helper}${NC}"
        exit 1
    fi
done

ensure_dnsmasq()
{
    command -v dnsmasq >/dev/null 2>&1 && return 0
    echo -e "${BLUE}Installing dnsmasq...${NC}"
    if command -v apt-get >/dev/null 2>&1; then
        DEBIAN_FRONTEND=noninteractive apt-get install -y dnsmasq ||
            { DEBIAN_FRONTEND=noninteractive apt-get update &&
              DEBIAN_FRONTEND=noninteractive apt-get install -y dnsmasq; }
    elif command -v opkg >/dev/null 2>&1; then
        opkg update && opkg install dnsmasq
    elif command -v dnf >/dev/null 2>&1; then
        dnf install -y dnsmasq
    elif command -v yum >/dev/null 2>&1; then
        yum install -y dnsmasq
    elif command -v apk >/dev/null 2>&1; then
        apk add dnsmasq
    else
        echo -e "${RED}Cannot install dnsmasq automatically; install it manually.${NC}"
        return 1
    fi
    command -v dnsmasq >/dev/null 2>&1
}

ensure_dnsmasq || exit 1

SERVER_PORT=8080
if [ -f "${VUPRS_DIR}/configs/server_config.json" ]; then
    PARSED_PORT=$(sed -n 's/.*"port"[[:space:]]*:[[:space:]]*"\([0-9][0-9]*\)".*/\1/p' \
        "${VUPRS_DIR}/configs/server_config.json" | head -n 1)
    [ -n "${PARSED_PORT}" ] && SERVER_PORT="${PARSED_PORT}"
fi

echo -e "${BLUE}============================================================${NC}"
echo -e "${BLUE} VUPRS one-key setup${NC}"
echo -e "${BLUE}============================================================${NC}"
echo -e "  Runtime dir : ${GREEN}${VUPRS_DIR}${NC}"
echo -e "  Interface   : ${GREEN}${IFACE}${NC}"
echo -e "  Board IP    : ${GREEN}${BOARD_IP}/24${NC}"
echo -e "  DHCP pool   : ${GREEN}${POOL_START} - ${POOL_END}${NC}"
echo -e "  Server port : ${GREEN}${SERVER_PORT}${NC}"
echo -e "  Start now   : ${GREEN}${DO_START}${NC}"
echo

if [ -n "${SSH_CONNECTION:-}" ]; then
    echo -e "${YELLOW}Warning: SSH session detected; changing the network may drop the connection.${NC}"
fi

# Stop the old server service so the port and the driver can be reconfigured.
systemctl stop vuprs-server.service 2>/dev/null || true

if command -v ss >/dev/null 2>&1 &&
   ss -ltn 2>/dev/null | awk 'NR > 1 {print $4}' | grep -qE "[:.]${SERVER_PORT}$"; then
    echo -e "${RED}TCP port ${SERVER_PORT} is already in use.${NC}"
    echo -e "${YELLOW}Stop the process using it (or change configs/server_config.json), then retry.${NC}"
    exit 1
fi

# 1. Configure the board IP.
if command -v nmcli >/dev/null 2>&1 && systemctl is-active --quiet NetworkManager; then
    CONN=$(nmcli -t -f NAME,DEVICE connection show --active |
           awk -F: -v dev="${IFACE}" '$2 == dev {print $1; exit}')
    if [ -z "${CONN}" ]; then
        echo -e "${RED}No active NetworkManager connection for ${IFACE}.${NC}"
        nmcli -t -f NAME,DEVICE connection show
        exit 1
    fi
    echo -e "${BLUE}Configuring ${IFACE} with NetworkManager (${CONN})...${NC}"
    nmcli connection modify "${CONN}" \
        connection.autoconnect yes \
        ipv4.method manual \
        ipv4.addresses "${BOARD_IP}/24" \
        ipv4.gateway "" \
        ipv4.dns ""
    nmcli connection up "${CONN}"
else
    echo -e "${BLUE}Configuring ${IFACE} with /etc/network/interfaces...${NC}"
    INTERFACE="${IFACE}" STATIC_IP="${BOARD_IP}" NETMASK="${NETMASK}" GATEWAY="" DNS="" \
        bash "${SCRIPT_DIR}/net_config.sh"
fi

for _ in {1..10}; do
    if ip -4 addr show dev "${IFACE}" 2>/dev/null | grep -q "inet ${BOARD_IP}/"; then
        break
    fi
    sleep 1
done
if ! ip -4 addr show dev "${IFACE}" 2>/dev/null | grep -q "inet ${BOARD_IP}/"; then
    echo -e "${RED}Board IP ${BOARD_IP} was not applied to ${IFACE}.${NC}"
    ip -4 addr show dev "${IFACE}" || true
    exit 1
fi
echo -e "${GREEN}Board IP is ${BOARD_IP}/24 on ${IFACE}.${NC}"

# 2. Configure and start DHCP.
echo -e "${BLUE}Configuring dnsmasq DHCP...${NC}"
BOARD_IP="${BOARD_IP}" \
DHCP_INTERFACE="${IFACE}" \
DHCP_POOL_START="${POOL_START}" \
DHCP_POOL_END="${POOL_END}" \
DHCP_LEASE="12h" \
    bash "${SCRIPT_DIR}/setup_dhcp.sh" || exit 1

# 3. Install and start the server service.
echo -e "${BLUE}Installing VUPRS server service...${NC}"
if [ "${DO_START}" -eq 1 ]; then
    bash "${SCRIPT_DIR}/install_server_service.sh" "${VUPRS_DIR}" --start || exit 1
else
    bash "${SCRIPT_DIR}/install_server_service.sh" "${VUPRS_DIR}" || exit 1
fi

# 4. Final verification.
echo
echo -e "${BLUE}Verification:${NC}"
VERIFY_FAILED=0
if systemctl is-active --quiet vuprs-dhcp.service; then
    echo -e "  ${GREEN}OK${NC} vuprs-dhcp.service is running"
else
    echo -e "  ${RED}FAIL${NC} vuprs-dhcp.service (journalctl -u vuprs-dhcp -n 50)"
    VERIFY_FAILED=1
fi
if [ "${DO_START}" -eq 1 ]; then
    if systemctl is-active --quiet vuprs-server.service; then
        echo -e "  ${GREEN}OK${NC} vuprs-server.service is running"
    else
        echo -e "  ${RED}FAIL${NC} vuprs-server.service (journalctl -u vuprs-server -n 50)"
        VERIFY_FAILED=1
    fi
    if command -v ss >/dev/null 2>&1; then
        if ss -ltn 2>/dev/null | awk 'NR > 1 {print $4}' | grep -qE "[:.]${SERVER_PORT}$"; then
            echo -e "  ${GREEN}OK${NC} TCP ${SERVER_PORT} is listening"
        else
            echo -e "  ${RED}FAIL${NC} TCP ${SERVER_PORT} is not listening"
            VERIFY_FAILED=1
        fi
    fi
else
    echo -e "  ${YELLOW}vuprs-server.service installed but not started (--no-start)${NC}"
fi
if command -v ss >/dev/null 2>&1 &&
   ss -lun 2>/dev/null | grep -qE ':(67)([[:space:]]|$)'; then
    echo -e "  ${GREEN}OK${NC} DHCP is listening on UDP 67"
else
    echo -e "  ${YELLOW}WARN${NC} no UDP 67 listener"
fi

echo
if [ "${VERIFY_FAILED}" -eq 0 ]; then
    echo -e "${GREEN}Done.${NC}"
else
    echo -e "${RED}Setup completed with errors; check the service logs above.${NC}"
fi
echo -e "  Windows: keep the Ethernet adapter on automatic DHCP."
echo -e "  Client target: ${BLUE}${BOARD_IP}:${SERVER_PORT}${NC}"
echo -e "  Server service: ${BLUE}systemctl status vuprs-server${NC}"
echo -e "  DHCP service  : ${BLUE}systemctl status vuprs-dhcp${NC}"
exit "${VERIFY_FAILED}"
