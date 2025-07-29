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

# Get the bus and device numbers from lsusb
USB_INFO=$(lsusb | grep "0cd5:0001")
BUS_NUM=$(echo "$USB_INFO" | sed 's/Bus \([0-9]*\) Device \([0-9]*\).*/\1/')
DEV_NUM=$(echo "$USB_INFO" | sed 's/Bus \([0-9]*\) Device \([0-9]*\).*/\2/')

echo "USB Bus: $BUS_NUM, Device: $DEV_NUM"

# Construct the interface path for WSL2/Linux
# Format is typically: bus-device:config.interface
INTERFACE_PATH="${BUS_NUM}-${DEV_NUM}:1.0"

echo "Trying interface path: $INTERFACE_PATH"

# Method 1: Try to unbind from usbhid driver
echo "Attempting to unbind from usbhid driver..."
if echo "$INTERFACE_PATH" | sudo tee /sys/bus/usb/drivers/usbhid/unbind >/dev/null 2>&1; then
    echo -e "${GREEN}✅ Successfully unbound from usbhid driver${NC}"
else
    echo -e "${YELLOW}⚠️  Could not unbind from usbhid (may not be bound or path incorrect)${NC}"
    
    # Try alternative interface path formats
    ALT_PATHS=("1-${DEV_NUM}:1.0" "${BUS_NUM}-1:1.0" "1-1:1.0")
    
    for alt_path in "${ALT_PATHS[@]}"; do
        echo "Trying alternative path: $alt_path"
        if echo "$alt_path" | sudo tee /sys/bus/usb/drivers/usbhid/unbind >/dev/null 2>&1; then
            echo -e "${GREEN}✅ Successfully unbound from usbhid driver using $alt_path${NC}"
            INTERFACE_PATH="$alt_path"
            break
        fi
    done
fi

# Method 2: Try to unbind from hid-generic driver
echo "Attempting to unbind from hid-generic driver..."

# Find HID device with LabJack VID:PID
HID_DEVICE=""
if [ -d /sys/bus/hid/devices ]; then
    for hid_dev in /sys/bus/hid/devices/*; do
        if [ -f "$hid_dev/uevent" ]; then
            if grep -q "HID_ID=0003:00000CD5:00000001" "$hid_dev/uevent" 2>/dev/null; then
                HID_DEVICE=$(basename "$hid_dev")
                break
            fi
        fi
    done
fi

if [ -n "$HID_DEVICE" ]; then
    echo "Found HID device: $HID_DEVICE"
    if echo "$HID_DEVICE" | sudo tee /sys/bus/hid/drivers/hid-generic/unbind >/dev/null 2>&1; then
        echo -e "${GREEN}✅ Successfully unbound from hid-generic driver${NC}"
    else
        echo -e "${YELLOW}⚠️  Could not unbind from hid-generic${NC}"
    fi
else
    echo -e "${YELLOW}⚠️  No matching HID device found${NC}"
fi

# Method 3: Try to remove hidraw device directly
if [ -e /dev/hidraw0 ]; then
    echo "Attempting to remove hidraw device..."
    
    # Find the hidraw device's parent and unbind it
    HIDRAW_PATH=$(readlink -f /sys/class/hidraw/hidraw0/device 2>/dev/null || echo "")
    if [ -n "$HIDRAW_PATH" ]; then
        HIDRAW_DRIVER=$(basename "$(readlink -f "$HIDRAW_PATH/driver" 2>/dev/null)" 2>/dev/null || echo "")
        HIDRAW_DEVICE=$(basename "$HIDRAW_PATH")
        
        if [ -n "$HIDRAW_DRIVER" ] && [ -n "$HIDRAW_DEVICE" ]; then
            echo "Trying to unbind $HIDRAW_DEVICE from $HIDRAW_DRIVER"
            if echo "$HIDRAW_DEVICE" | sudo tee "/sys/bus/hid/drivers/$HIDRAW_DRIVER/unbind" >/dev/null 2>&1; then
                echo -e "${GREEN}✅ Successfully unbound hidraw device${NC}"
            fi
        fi
    fi
fi

# Wait a moment for the system to process the unbinding
sleep 1

# Check results
echo ""
echo "=== Unbinding Results ==="

if [ -e /dev/hidraw0 ]; then
    echo -e "${YELLOW}⚠️  /dev/hidraw0 still exists - HID driver may still be bound${NC}"
    echo "This may still work if the driver was detached from the interface"
else
    echo -e "${GREEN}✅ HID device removed successfully${NC}"
fi

# Set USB device permissions
echo "Setting USB device permissions..."
USB_DEV_PATH="/dev/bus/usb/$(printf "%03d" "$BUS_NUM")/$(printf "%03d" "$DEV_NUM")"
if [ -e "$USB_DEV_PATH" ]; then
    sudo chmod 666 "$USB_DEV_PATH" 2>/dev/null || true
    echo -e "${GREEN}✅ USB permissions set for $USB_DEV_PATH${NC}"
else
    echo -e "${YELLOW}⚠️  USB device path not found: $USB_DEV_PATH${NC}"
fi

echo ""
echo -e "${GREEN}🎉 HID driver unbinding attempt complete!${NC}"
echo ""
echo "You can now test the LabJack U12 driver:"
echo "  sigrok-cli -d labjack-u12 --scan"
echo ""
