#!/bin/bash

# Quick LabJack U12 Driver Test
# Tests the critical fixes for HID driver conflicts

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}LabJack U12 Quick Test${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Check if LabJack is connected
echo -e "${YELLOW}1. Checking LabJack U12 connection...${NC}"
if lsusb | grep -q "0cd5:0001"; then
    echo -e "${GREEN}✅ LabJack U12 detected${NC}"
else
    echo -e "${RED}❌ LabJack U12 not found${NC}"
    exit 1
fi

# Check HID driver status
echo -e "${YELLOW}2. Checking HID driver status...${NC}"
if [ -e /dev/hidraw0 ]; then
    echo -e "${YELLOW}⚠️  HID driver is bound (/dev/hidraw0 exists)${NC}"
    echo "Attempting to unbind HID driver..."
    ./unbind_labjack_hid.sh
else
    echo -e "${GREEN}✅ HID driver not bound${NC}"
fi

# Test driver scan
echo -e "${YELLOW}3. Testing driver scan...${NC}"
if sigrok-cli -d labjack-u12 --scan >/dev/null 2>&1; then
    echo -e "${GREEN}✅ Driver scan successful${NC}"
    sigrok-cli -d labjack-u12 --scan
else
    echo -e "${RED}❌ Driver scan failed${NC}"
    echo "Running with verbose output:"
    sigrok-cli -d labjack-u12 --scan -l 5
    exit 1
fi

# Test device opening
echo -e "${YELLOW}4. Testing device opening...${NC}"
if sigrok-cli -d labjack-u12 --config device-mode=single-ended --channels AI0 --samples 1 -l 5 >/dev/null 2>&1; then
    echo -e "${GREEN}✅ Device opens successfully${NC}"
else
    echo -e "${RED}❌ Device opening failed${NC}"
    echo "Running with verbose output:"
    sigrok-cli -d labjack-u12 --config device-mode=single-ended --channels AI0 --samples 1 -l 5
    exit 1
fi

# Test basic acquisition
echo -e "${YELLOW}5. Testing basic data acquisition...${NC}"
echo "Reading AI0 channel (5 samples):"
if sigrok-cli -d labjack-u12 --config device-mode=single-ended --channels AI0 --samples 5; then
    echo -e "${GREEN}✅ Data acquisition successful${NC}"
else
    echo -e "${RED}❌ Data acquisition failed${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}🎉 All tests passed!${NC}"
echo -e "${GREEN}LabJack U12 driver is working correctly${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo "You can now run the full test suite:"
echo "  ./labjack_u12_testsuite.sh"
echo ""
