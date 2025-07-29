# LabJack U12 Driver Test Suite

This comprehensive test suite is designed for remote hardware testing and development of the LabJack U12 libsigrok driver.

## 🎯 Purpose

Since the developer doesn't have direct access to LabJack U12 hardware, this test suite:
- **Captures detailed logs** for remote analysis
- **Tests all driver functionality** systematically  
- **Identifies issues** with specific error context
- **Provides performance metrics** for optimization
- **Generates actionable recommendations** for fixes

## 📋 Test Coverage

### Core Functionality
- ✅ Driver loading and initialization
- ✅ Device detection and scanning
- ✅ USB communication and interface claiming
- ✅ Configuration management (single-ended/differential modes)

### Analog Input Testing
- ✅ Single channel acquisition (AI0-AI7)
- ✅ Multi-channel acquisition
- ✅ Differential mode testing
- ✅ Voltage range validation

### Digital I/O Testing  
- ✅ IO channels (IO0-IO3) reading
- ✅ D channels (D0-D15) reading
- ✅ Mixed digital channel acquisition
- ✅ Pattern mode configuration

### Advanced Features
- ✅ Counter functionality (CNT channel)
- ✅ Mixed analog/digital acquisition
- ✅ Sample limiting and timing
- ✅ Continuous acquisition mode

### Error Handling
- ✅ Invalid channel specifications
- ✅ Configuration conflicts
- ✅ USB permission issues
- ✅ HID driver conflicts

### Performance Testing
- ✅ Acquisition timing measurements
- ✅ Multi-channel performance
- ✅ Stress testing with rapid start/stop
- ✅ Long acquisition testing

## 🚀 Quick Start

### Prerequisites

1. **LabJack U12 hardware** connected via USB
2. **libsigrok with LabJack U12 driver** installed
3. **sigrok-cli** available in PATH
4. **Basic Linux utilities** (lsusb, dmesg, etc.)

### Installation

```bash
# Clone the repository
git clone https://github.com/tridentsx/libsigrok.git
cd libsigrok
git checkout labjack-u12-driver

# Build and install the driver
./autogen.sh
./configure --enable-labjack-u12
make -j4
sudo make install
sudo ldconfig

# Install udev rules for USB permissions
sudo cp 99-labjack-u12.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules
sudo udevadm trigger

# Disconnect and reconnect LabJack U12
```

### Running the Test Suite

```bash
# Make sure LabJack U12 is connected
lsusb | grep 0cd5

# Run the comprehensive test suite
./labjack_u12_testsuite.sh

# Results will be saved in timestamped directory
# Example: labjack_u12_test_logs_20241201_143022/
```

## 📊 Understanding Results

### Test Results
- ✅ **PASS**: Test completed successfully
- ❌ **FAIL**: Test failed unexpectedly (needs investigation)
- ⚠️ **EXPECTED_FAIL**: Test failed as expected (invalid input, etc.)
- 🔄 **UNEXPECTED_SUCCESS**: Test passed when failure was expected

### Log Files Generated

| File | Description |
|------|-------------|
| `system_info.log` | System configuration, USB devices, kernel info |
| `libsigrok_info.log` | LibSigrok version and driver information |
| `01_driver_scan.log` | Basic device detection test |
| `08_ai_single_channel.log` | Single analog input test |
| `12_digital_io_channels.log` | Digital I/O functionality test |
| `test_summary.log` | Overall test results summary |

### Archive Contents
The test suite creates a compressed archive (`*.tar.gz`) containing:
- All individual test logs
- System information
- Performance metrics  
- Summary report
- Error analysis

## 🔧 Troubleshooting Common Issues

### "No devices found"
```bash
# Check USB connection
lsusb | grep 0cd5

# Check permissions
ls -la /dev/bus/usb/*/

# Check for HID driver conflicts
dmesg | grep -i labjack
```

### "Failed to claim USB interface"
```bash
# Unbind HID driver manually
echo "1-1:1.0" | sudo tee /sys/bus/usb/drivers/usbhid/unbind

# Check interface status
lsusb -v -d 0cd5:0001 | grep -A5 bInterfaceClass
```

### "Permission denied"
```bash
# Add user to plugdev group
sudo usermod -a -G plugdev $USER

# Reload udev rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# Log out and back in
```

## 📈 Analyzing Results

### Automated Analysis
```bash
# Run the log analyzer (requires Python 3)
python3 analyze_test_logs.py labjack_u12_test_logs_YYYYMMDD_HHMMSS/

# This generates:
# - Console summary
# - analysis_report.json with detailed findings
# - Prioritized recommendations
```

### Manual Analysis
Key files to examine for issues:

1. **test_summary.log** - Overall results and failure count
2. **system_info.log** - Hardware detection and USB status  
3. **Failed test logs** - Specific error messages and context
4. **01_driver_scan.log** - Basic driver functionality

### Critical Indicators

🚨 **High Priority Issues:**
- Any test with `RESULT: FAIL`
- USB communication errors (`LIBUSB_ERROR_*`)
- Segmentation faults
- Device not detected in system_info.log

⚠️ **Medium Priority Issues:**
- HID driver conflicts
- Permission warnings
- Timeout errors
- Performance degradation

## 📤 Sending Results to Developer

### What to Send
1. **Complete archive**: `labjack_u12_test_logs_YYYYMMDD_HHMMSS.tar.gz`
2. **Analysis report**: `analysis_report.json` (if generated)
3. **Brief description** of your setup and any observed issues

### Information to Include
- **Hardware**: LabJack U12 model/version
- **OS**: Distribution and kernel version
- **Connection**: USB port, hub, cable details
- **Symptoms**: What works/doesn't work from user perspective

### Example Email
```
Subject: LabJack U12 Driver Test Results - [SUCCESS/ISSUES]

Hi,

I've run the comprehensive test suite for the LabJack U12 driver.

Setup:
- Hardware: LabJack U12 (firmware X.XX)
- OS: Ubuntu 22.04 LTS (kernel 5.15.0)
- Connection: Direct USB 2.0 connection

Results Summary:
- Total Tests: XX
- Passed: XX  
- Failed: XX
- Critical Issues: XX

[Brief description of any major issues observed]

Attached: labjack_u12_test_logs_YYYYMMDD_HHMMSS.tar.gz

Thanks!
```

## 🔄 Iterative Testing

After receiving fixes from the developer:

1. **Pull latest changes**:
   ```bash
   git pull origin labjack-u12-driver
   make -j4
   sudo make install
   ```

2. **Re-run specific tests**:
   ```bash
   # Run only failed tests
   sigrok-cli -d labjack-u12 --scan -l 5
   ```

3. **Full regression testing**:
   ```bash
   ./labjack_u12_testsuite.sh
   ```

## 🎯 Expected Outcomes

### Successful Test Run
- **95%+ pass rate** on functional tests
- **Device detected** in system scan
- **Clean USB communication** without errors
- **All channel types working** (AI, AO, IO, D, CNT)
- **Configuration changes applied** successfully

### Common Development Issues
- **USB permission/HID conflicts** (fixable with udev rules)
- **Channel configuration bugs** (differential mode conflicts)
- **Data acquisition timing** (poll rate optimization)
- **Error handling gaps** (edge cases not covered)

This test suite provides comprehensive coverage to identify and resolve these issues systematically.

---

**Happy Testing! 🚀**

*This test suite is designed to accelerate LabJack U12 driver development through comprehensive remote testing and detailed issue reporting.*
