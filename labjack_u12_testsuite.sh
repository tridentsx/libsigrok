#!/bin/bash

# LabJack U12 Driver Test Suite
# Comprehensive testing and logging for remote development
# Run this on the machine with LabJack U12 hardware connected

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
LOGDIR="labjack_u12_test_logs_$(date +%Y%m%d_%H%M%S)"
# Use our compiled library instead of system-installed one
export LD_LIBRARY_PATH="$(pwd)/.libs:$LD_LIBRARY_PATH"
SIGROK_CLI="sigrok-cli"
TEST_SAMPLES=10
VERBOSE_LEVEL=5

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}LabJack U12 Driver Test Suite${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Create log directory
mkdir -p "$LOGDIR"
cd "$LOGDIR"

# Function to run command and log output
run_test() {
    local test_name="$1"
    local command="$2"
    local log_file="${test_name}.log"
    local expected_success="$3"  # true/false
    
    echo -e "${YELLOW}Running: $test_name${NC}"
    echo "Command: $command" > "$log_file"
    echo "Timestamp: $(date)" >> "$log_file"
    echo "========================================" >> "$log_file"
    
    if timeout 30s bash -c "$command" >> "$log_file" 2>&1; then
        if [ "$expected_success" = "true" ]; then
            echo -e "${GREEN}✅ PASS: $test_name${NC}"
            echo "RESULT: PASS" >> "$log_file"
        else
            echo -e "${YELLOW}⚠️  UNEXPECTED SUCCESS: $test_name${NC}"
            echo "RESULT: UNEXPECTED_SUCCESS" >> "$log_file"
        fi
    else
        if [ "$expected_success" = "false" ]; then
            echo -e "${GREEN}✅ EXPECTED FAIL: $test_name${NC}"
            echo "RESULT: EXPECTED_FAIL" >> "$log_file"
        else
            echo -e "${RED}❌ FAIL: $test_name${NC}"
            echo "RESULT: FAIL" >> "$log_file"
        fi
    fi
    echo ""
}

# System Information
echo -e "${BLUE}=== System Information ===${NC}"
{
    echo "=== SYSTEM INFO ==="
    echo "Date: $(date)"
    echo "Hostname: $(hostname)"
    echo "Kernel: $(uname -a)"
    echo "Distribution: $(lsb_release -d 2>/dev/null || echo 'Unknown')"
    echo "USB Utils Version: $(lsusb --version 2>/dev/null || echo 'Not installed')"
    echo ""
    
    echo "=== USB DEVICES ==="
    lsusb 2>/dev/null || echo "lsusb not available"
    echo ""
    
    echo "=== LABJACK U12 USB DETAILS ==="
    lsusb -v -d 0cd5:0001 2>/dev/null || echo "LabJack U12 not found or lsusb not available"
    echo ""
    
    echo "=== USB DEVICE TREE ==="
    find /sys/bus/usb/devices -name "*0cd5*" -o -name "*0001*" 2>/dev/null | head -10
    echo ""
    
    echo "=== HID DEVICES ==="
    ls -la /dev/hidraw* 2>/dev/null || echo "No HID devices found"
    echo ""
    
    echo "=== DMESG LABJACK ENTRIES ==="
    dmesg | grep -i "labjack\|0cd5\|0001" | tail -20 || echo "No LabJack entries in dmesg"
    echo ""
    
} > system_info.log

# LibSigrok Information
echo -e "${BLUE}=== LibSigrok Information ===${NC}"
{
    echo "=== LIBSIGROK VERSION ==="
    sigrok-cli --version 2>/dev/null || echo "sigrok-cli not found"
    echo ""
    
    echo "=== AVAILABLE DRIVERS ==="
    sigrok-cli --list-drivers 2>/dev/null | grep -E "(labjack|u12)" || echo "No LabJack drivers found"
    echo ""
    
    echo "=== ALL DRIVERS ==="
    sigrok-cli --list-drivers 2>/dev/null || echo "Cannot list drivers"
    echo ""
    
} > libsigrok_info.log

# Core Driver Tests
echo -e "${BLUE}=== Core Driver Tests ===${NC}"

run_test "01_driver_scan" \
    "$SIGROK_CLI -d labjack-u12 --scan -l $VERBOSE_LEVEL" \
    "true"

run_test "02_driver_scan_verbose" \
    "$SIGROK_CLI -d labjack-u12 --scan -l 5" \
    "true"

run_test "03_list_device_options" \
    "$SIGROK_CLI -d labjack-u12 --show" \
    "true"

# Configuration Tests
echo -e "${BLUE}=== Configuration Tests ===${NC}"

run_test "04_set_single_ended_mode" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=single-ended -l $VERBOSE_LEVEL" \
    "true"

run_test "05_set_differential_mode" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=differential -l $VERBOSE_LEVEL" \
    "true"

run_test "06_set_sample_limit" \
    "$SIGROK_CLI -d labjack-u12 --config limit_samples=100 -l $VERBOSE_LEVEL" \
    "true"

run_test "07_invalid_config" \
    "$SIGROK_CLI -d labjack-u12 --config invalid-option=test -l $VERBOSE_LEVEL" \
    "false"

# Analog Input Tests
echo -e "${BLUE}=== Analog Input Tests ===${NC}"

run_test "08_ai_single_channel" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=single-ended --channels AI0 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "09_ai_multiple_channels" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=single-ended --channels AI0,AI1,AI2 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "10_ai_all_channels" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=single-ended --channels AI0,AI1,AI2,AI3,AI4,AI5,AI6,AI7 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "11_ai_differential_mode" \
    "$SIGROK_CLI -d labjack-u12 --config device_mode=differential --channels AI0,AI2 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

# Digital I/O Tests
echo -e "${BLUE}=== Digital I/O Tests ===${NC}"

run_test "12_digital_io_channels" \
    "$SIGROK_CLI -d labjack-u12 --channels IO0,IO1,IO2,IO3 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "13_digital_d_channels" \
    "$SIGROK_CLI -d labjack-u12 --channels D0,D1,D2,D3 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "14_mixed_digital_channels" \
    "$SIGROK_CLI -d labjack-u12 --channels IO0,D0,D1,D15 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

# Counter Tests
echo -e "${BLUE}=== Counter Tests ===${NC}"

run_test "15_counter_channel" \
    "$SIGROK_CLI -d labjack-u12 --channels CNT --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

# Mixed Channel Tests
echo -e "${BLUE}=== Mixed Channel Tests ===${NC}"

run_test "16_analog_digital_mixed" \
    "$SIGROK_CLI -d labjack-u12 --channels AI0,AI1,IO0,D0 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

run_test "17_all_channel_types" \
    "$SIGROK_CLI -d labjack-u12 --channels AI0,AI1,IO0,D0,CNT --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "true"

# Stress Tests
echo -e "${BLUE}=== Stress Tests ===${NC}"

run_test "18_long_acquisition" \
    "$SIGROK_CLI -d labjack-u12 --channels AI0 --samples 100 -l $VERBOSE_LEVEL" \
    "true"

run_test "19_rapid_start_stop" \
    "for i in {1..5}; do $SIGROK_CLI -d labjack-u12 --channels AI0 --samples 5 -l $VERBOSE_LEVEL; done" \
    "true"

# Error Condition Tests
echo -e "${BLUE}=== Error Condition Tests ===${NC}"

run_test "20_invalid_channel" \
    "$SIGROK_CLI -d labjack-u12 --channels AI99 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "false"

run_test "21_conflicting_differential" \
    "$SIGROK_CLI -d labjack-u12 --config device-mode=differential --channels AI1 --samples $TEST_SAMPLES -l $VERBOSE_LEVEL" \
    "false"

# Performance Tests
echo -e "${BLUE}=== Performance Tests ===${NC}"

run_test "22_timing_single_sample" \
    "time $SIGROK_CLI -d labjack-u12 --channels AI0 --samples 1 -l $VERBOSE_LEVEL" \
    "true"

run_test "23_timing_multiple_channels" \
    "time $SIGROK_CLI -d labjack-u12 --channels AI0,AI1,AI2,AI3 --samples 10 -l $VERBOSE_LEVEL" \
    "true"

# Hardware-specific Tests
echo -e "${BLUE}=== Hardware-specific Tests ===${NC}"

run_test "24_device_info" \
    "$SIGROK_CLI -d labjack-u12 --show -l $VERBOSE_LEVEL" \
    "true"

run_test "25_continuous_mode" \
    "timeout 10s $SIGROK_CLI -d labjack-u12 --channels AI0 --continuous -l $VERBOSE_LEVEL" \
    "true"

# Generate Summary Report
echo -e "${BLUE}=== Generating Summary Report ===${NC}"

{
    echo "========================================="
    echo "LabJack U12 Driver Test Summary"
    echo "========================================="
    echo "Test Date: $(date)"
    echo "Log Directory: $LOGDIR"
    echo ""
    
    echo "=== TEST RESULTS ==="
    total_tests=0
    passed_tests=0
    failed_tests=0
    
    for log_file in *.log; do
        if [[ "$log_file" != "system_info.log" && "$log_file" != "libsigrok_info.log" && "$log_file" != "test_summary.log" ]]; then
            total_tests=$((total_tests + 1))
            result=$(grep "RESULT:" "$log_file" | cut -d' ' -f2)
            test_name=$(basename "$log_file" .log)
            
            case "$result" in
                "PASS"|"EXPECTED_FAIL")
                    passed_tests=$((passed_tests + 1))
                    echo "✅ $test_name: $result"
                    ;;
                "FAIL"|"UNEXPECTED_SUCCESS")
                    failed_tests=$((failed_tests + 1))
                    echo "❌ $test_name: $result"
                    ;;
                *)
                    echo "❓ $test_name: UNKNOWN"
                    ;;
            esac
        fi
    done
    
    echo ""
    echo "=== SUMMARY ==="
    echo "Total Tests: $total_tests"
    echo "Passed: $passed_tests"
    echo "Failed: $failed_tests"
    echo "Success Rate: $(( passed_tests * 100 / total_tests ))%"
    echo ""
    
    echo "=== CRITICAL ISSUES ==="
    if grep -l "RESULT: FAIL" *.log >/dev/null 2>&1; then
        echo "The following tests failed unexpectedly:"
        grep -l "RESULT: FAIL" *.log | while read -r file; do
            echo "- $(basename "$file" .log)"
        done
    else
        echo "No critical failures detected!"
    fi
    echo ""
    
    echo "=== NEXT STEPS ==="
    echo "1. Review individual test logs for detailed error information"
    echo "2. Check system_info.log for hardware/system configuration"
    echo "3. Send entire log directory to developer for analysis"
    echo "4. Pay special attention to any FAIL results"
    echo ""
    
} > test_summary.log

# Create archive for easy transfer
echo -e "${BLUE}=== Creating Archive ===${NC}"
cd ..
tar -czf "${LOGDIR}.tar.gz" "$LOGDIR"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Test Suite Complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}Results saved to: ${LOGDIR}${NC}"
echo -e "${YELLOW}Archive created: ${LOGDIR}.tar.gz${NC}"
echo ""
echo -e "${BLUE}To send results to developer:${NC}"
echo "1. Send the entire archive: ${LOGDIR}.tar.gz"
echo "2. Or send the log directory: ${LOGDIR}/"
echo ""
echo -e "${BLUE}Quick summary:${NC}"
cat "${LOGDIR}/test_summary.log" | tail -20

echo ""
echo -e "${GREEN}Thank you for testing the LabJack U12 driver!${NC}"
