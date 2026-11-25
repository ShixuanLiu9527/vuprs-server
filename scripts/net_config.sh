#!/bin/bash

# ---------------------------- Ethernet Config -------------------------------
INTERFACE="eth0"
STATIC_IP="192.168.1.100"
NETMASK="255.255.255.0"
GATEWAY="192.168.1.1"
DNS="114.114.114.114"
# ----------------------------------------------------------------------------

RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

NETWORK_INTERFACES_CONFIG="${NETWORK_INTERFACES_CONFIG:-/etc/network/interfaces}"
BACKUP_FILE="${NETWORK_INTERFACES_CONFIG}.bak_vuprs"

# Writing /etc/network/interfaces and restarting networking requires root.
if [[ $EUID -ne 0 ]]; then
    echo -e "${RED}This script must be run as root.${NC}"
    exit 1
fi

if ! command -v ip >/dev/null 2>&1; then
    echo -e "${RED}ip command not found; cannot configure or verify the network.${NC}"
    exit 1
fi

mkdir -p "$(dirname "${NETWORK_INTERFACES_CONFIG}")"

# Back up the original configuration once.
if [ -f "${NETWORK_INTERFACES_CONFIG}" ] && [ ! -f "${BACKUP_FILE}" ]; then
    if cp "${NETWORK_INTERFACES_CONFIG}" "${BACKUP_FILE}"; then
        echo -e "${GREEN}Reserved network interfaces to file: ${BACKUP_FILE}${NC}"
    else
        echo -e "${RED}Failed to back up ${NETWORK_INTERFACES_CONFIG}.${NC}"
        exit 1
    fi
fi

# Change static IP
echo "Config options:"
echo -e "  Static IP: ${BLUE}${STATIC_IP}${NC}"
echo -e "  Netmask: ${BLUE}${NETMASK}${NC}"
echo -e "  Gateway: ${BLUE}${GATEWAY}${NC}"
echo -e "  DNS: ${BLUE}${DNS}${NC}"
if ! cat > "${NETWORK_INTERFACES_CONFIG}" <<EOF
auto lo
iface lo inet loopback

auto ${INTERFACE}
iface ${INTERFACE} inet static
    address ${STATIC_IP}
    netmask ${NETMASK}
    gateway ${GATEWAY}
    dns-nameservers ${DNS}
EOF
then
    echo -e "${RED}Failed to write ${NETWORK_INTERFACES_CONFIG}.${NC}"
    exit 1
fi
echo -e "${GREEN}Successfully save config to ${NETWORK_INTERFACES_CONFIG}${NC}"
echo -e "${BLUE}Restart ethernet ...${NC}"

# Restart network
if command -v systemctl >/dev/null 2>&1; then
    if systemctl restart networking; then
        echo -e "${GREEN}Network restarted successfully (systemctl)${NC}"
    else
        echo -e "${RED}Failed to restart network with systemctl${NC}"
        exit 1
    fi
elif [ -f /etc/init.d/networking ]; then
    if /etc/init.d/networking restart; then
        echo -e "${GREEN}Network restarted successfully (init.d)${NC}"
    else
        echo -e "${RED}Failed to restart network with init.d${NC}"
        exit 1
    fi
else
    echo -e "${RED}Cannot restart network automatically.${NC}"
    echo -e "${YELLOW}Please reboot the system manually.${NC}"
fi

echo -e "${BLUE}Please wait for network to restart...${NC}"
sleep 2

echo -e "${BLUE}Verifying IP address...${NC}"
CURRENT_IP=$(ip -4 addr show dev "${INTERFACE}" 2>/dev/null | awk '/inet /{sub(/\/.*/, "", $2); print $2; exit}')
if [ "${CURRENT_IP}" = "${STATIC_IP}" ]; then
    echo -e "${GREEN}IP successfully set to ${STATIC_IP}${NC}"
else
    echo -e "${RED}IP verification failed. Current IP: ${CURRENT_IP:-none}${NC}"
fi

echo -e "${GREEN}Ethernet config DONE.${NC}"
