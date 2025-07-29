#!/bin/bash

# LabJack U12 HID Driver Unbinding Script
# This script unbinds the HID driver from LabJack U12 to allow libsigrok access

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}LabJack U12 HID Driver Unbinding Script${NC}"
echo "========================================"

# Check if LabJack U12 is connected
if ! lsusb | grep -q "0cd5:0001"; then
    echo -e "${RED}❌ LabJack U12 not found. Please connect the device.${NC}"
    exit 1
fi

echo -e "${GREEN}✅ LabJack U12 detected${NC}"

# Find the USB device path
USB_DEVICE=$(find /sys/bus/usb/devices -name "*0cd5*" -o -name "*0001*" | head -1)
if [ -z "$USB_DEVICE" ]; then
    echo -e "${RED}❌ Cannot find USB device path${NC}"
    exit 1
fi

echo "USB device path: $USB_DEVICE"

# Find the interface path
INTERFACE_PATH=""
for path in /sys/bus/usb/devices/*/; do
    if [ -f "$path/idVendor" ] && [ -f "$path/idProduct" ]; then
        vendor=$(cat "$path/idVendor" 2>/dev/null)
        product=$(cat "$path/idProduct" 2>/dev/null)
        if [ "$vendor" = "0cd5" ] && [ "$product" = "0001" ]; then
            # Found the device, now find its interface
            device_name=$(basename "$path")
            INTERFACE_PATH="${device_name}:1.0"
            break
        fi
    fi
done

if [ -z "$INTERFACE_PATH" ]; then
    echo -e "${RED}❌ Cannot determine interface path${NC}"
    exit 1
fi

echo "Interface path: $INTERFACE_PATH"

# Unbind from usbhid driver
echo "Attempting to unbind from usbhid driver..."
if echo "$INTERFACE_PATH" | sudo tee /sys/bus/usb/drivers/usbhid/unbind >/dev/null 2>&1; then
    echo -e "${GREEN}✅ Successfully unbound from usbhid driver${NC}"
else
    echo -e "${YELLOW}⚠️  Could not unbind from usbhid (may not be bound)${NC}"
fi

# Unbind from hid-generic driver
echo "Attempting to unbind from hid-generic driver..."
HID_DEVICE=$(find /sys/bus/hid/devices -name "*0CD5:0001*" | head -1)
if [ -n "$HID_DEVICE" ]; then
    HID_NAME=$(basename "$HID_DEVICE")
    if echo "$HID_NAME" | sudo tee /sys/bus/hid/drivers/hid-generic/unbind >/dev/null 2>&1; then
        echo -e "${GREEN}✅ Successfully unbound from hid-generic driver${NC}"
    else
        echo -e "${YELLOW}⚠️  Could not unbind from hid-generic${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  No HID device found to unbind${NC}"
fi

# Check if hidraw device still exists
if [ -e /dev/hidraw0 ]; then
    echo -e "${YELLOW}⚠️  /dev/hidraw0 still exists - HID driver may still be bound${NC}"
else
    echo -e "${GREEN}✅ HID device removed successfully${NC}"
fi

# Set USB device permissions
echo "Setting USB device permissions..."
USB_DEV_PATH=$(find /dev/bus/usb -name "*" -exec grep -l "0cd5.*0001" {} \; 2>/dev/null | head -1)
if [ -n "$USB_DEV_PATH" ]; then
    sudo chmod 666 "$USB_DEV_PATH" 2>/dev/null || true
    echo -e "${GREEN}✅ USB permissions set${NC}"
fi

echo ""
echo -e "${GREEN}🎉 HID driver unbinding complete!${NC}"
echo ""
echo "You can now test the LabJack U12 driver:"
echo "  sigrok-cli -d labjack-u12 --scan"
echo ""
