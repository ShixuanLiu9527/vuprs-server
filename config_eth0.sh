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
BLUE='\033[1;34m'
NC='\033[0m'  # No Color

NETWORK_INTERFACES_CONFIG=/etc/network/interfaces
BACKUP_FILE="${NETWORK_INTERFACES_CONFIG}.bak_vuprs"  # Reserved

if [ ! -f "${BACKUP_FILE}" ]; then
    cp "${NETWORK_INTERFACES_CONFIG}" "${BACKUP_FILE}"
    echo -e "${GREEN}Reserve network interfaces to file: ${BACKUP_FILE}${NC}"
fi

# Change static IP

echo "Config options:"
echo -e "  Static IP: ${BLUE}${STATIC_IP}${NC}"
echo -e "  Netmask: ${BLUE}${NETMASK}${NC}"
echo -e "  Gateway: ${BLUE}${GATEWAY}${NC}"
echo -e "  DNS: ${BLUE}${DNS}${NC}"

cat > "${NETWORK_INTERFACES_CONFIG}" <<EOF
auto lo
iface lo inet loopback

auto ${INTERFACE}
iface ${INTERFACE} inet static
    address ${STATIC_IP}
    netmask ${NETMASK}
    gateway ${GATEWAY}
    dns-nameservers ${DNS}
EOF

echo -e "${GREEN}Successfully save config to ${NETWORK_INTERFACES_CONFIG}${NC}"
echo -e "${BLUE}Restart ethernet ...${NC}"

# Restart network

if command -v systemctl &> /dev/null; then
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
CURRENT_IP=$(ip -4 addr show ${INTERFACE} 2>/dev/null | grep -oP '(?<=inet\s)\d+(\.\d+){3}' | head -1)

if [ "$CURRENT_IP" = "$STATIC_IP" ]; then
    echo -e "${GREEN}IP successfully set to ${STATIC_IP}${NC}"
else
    echo -e "${RED}IP verification failed. Current IP: ${CURRENT_IP:-'none'}${NC}"
fi

echo -e "${GREEN}Ethernet config DONE.${NC}"
