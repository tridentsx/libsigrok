/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2025 Carl-Fredrik Sundstrom <carl.f.sundstrom@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <libusb.h>
#include "protocol.h"

/**
 * Check if an AI channel is available in the current mode.
 * 
 * @param devc Device context
 * @param ai_index AI channel index (0-7)
 * @return TRUE if channel is available, FALSE otherwise
 */
SR_PRIV bool labjack_u12_is_ai_channel_available(const struct dev_context *devc, int ai_index)
{
	if (ai_index < 0 || ai_index > 7)
		return FALSE;

	if (devc->ai_mode == AI_MODE_SINGLE_ENDED) {
		/* In single-ended mode, all AI channels are available */
		return TRUE;
	} else {
		/* In differential mode, only even-numbered channels are available as pairs */
		return (ai_index % 2 == 0);
	}
}

/**
 * Get the differential pair index for an AI channel.
 * 
 * @param ai_index AI channel index (0-7)
 * @return Differential pair index (0-3), or -1 if invalid
 */
SR_PRIV int labjack_u12_get_differential_pair(int ai_index)
{
	if (ai_index < 0 || ai_index > 7)
		return -1;
	
	return ai_index / 2;
}

/**
 * Check if enabling an AI channel would conflict with current configuration.
 * 
 * @param devc Device context
 * @param ai_index AI channel index (0-7)
 * @return TRUE if there would be a conflict, FALSE otherwise
 */
SR_PRIV bool labjack_u12_ai_channels_conflict(const struct dev_context *devc, int ai_index)
{
	int pair_index;

	if (ai_index < 0 || ai_index > 7)
		return TRUE;

	if (devc->ai_mode == AI_MODE_SINGLE_ENDED) {
		/* In single-ended mode, no conflicts between individual channels */
		return FALSE;
	} else {
		/* In differential mode, check if the pair is already enabled */
		pair_index = labjack_u12_get_differential_pair(ai_index);
		if (pair_index < 0 || pair_index > 3)
			return TRUE;
		
		/* Only even-numbered channels can be enabled in differential mode */
		if (ai_index % 2 != 0)
			return TRUE;
		
		return FALSE;
	}
}

/**
 * Write data to LabJack U12 via USB.
 * 
 * @param sdi Device instance
 * @param data Data to write
 * @param length Length of data
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_usb_write(const struct sr_dev_inst *sdi, 
                                  const void *data, size_t length)
{
	struct dev_context *devc;
	int transferred, ret;

	if (!sdi || !sdi->priv || !data || length == 0) {
		sr_err("Invalid parameters for USB write");
		return SR_ERR_ARG;
	}

	devc = sdi->priv;
	if (!devc->usb || !devc->usb->devhdl) {
		sr_err("USB device not open or invalid");
		return SR_ERR_DEV_CLOSED;
	}

	sr_spew("USB write: %zu bytes to endpoint 0x%02x", length, LABJACK_USB_ENDPOINT_OUT);

	g_mutex_lock(&devc->usb_mutex);
	
	ret = libusb_interrupt_transfer(devc->usb->devhdl, 
	                               LABJACK_USB_ENDPOINT_OUT,
	                               (unsigned char *)data, 
	                               length,
	                               &transferred, 
	                               LABJACK_USB_TIMEOUT_MS);
	
	g_mutex_unlock(&devc->usb_mutex);

	if (ret != LIBUSB_SUCCESS) {
		sr_err("USB write failed: %s (endpoint 0x%02x, %zu bytes)", 
		       libusb_error_name(ret), LABJACK_USB_ENDPOINT_OUT, length);
		return SR_ERR;
	}

	if (transferred != (int)length) {
		sr_err("USB write incomplete: %d/%zu bytes", transferred, length);
		return SR_ERR;
	}

	sr_spew("USB write successful: %d bytes", transferred);
	return SR_OK;
}

/**
 * Read data from LabJack U12 via USB.
 * 
 * @param sdi Device instance
 * @param data Buffer to read into
 * @param length Length of data to read
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_usb_read(const struct sr_dev_inst *sdi, 
                                 void *data, size_t length)
{
	struct dev_context *devc;
	int transferred, ret;

	if (!sdi || !sdi->priv || !data || length == 0) {
		sr_err("Invalid parameters for USB read");
		return SR_ERR_ARG;
	}

	devc = sdi->priv;
	if (!devc->usb || !devc->usb->devhdl) {
		sr_err("USB device not open or invalid");
		return SR_ERR_DEV_CLOSED;
	}

	sr_spew("USB read: %zu bytes from endpoint 0x%02x", length, LABJACK_USB_ENDPOINT_IN);

	g_mutex_lock(&devc->usb_mutex);
	
	ret = libusb_interrupt_transfer(devc->usb->devhdl, 
	                               LABJACK_USB_ENDPOINT_IN,
	                               (unsigned char *)data, 
	                               length,
	                               &transferred, 
	                               LABJACK_USB_TIMEOUT_MS);
	
	g_mutex_unlock(&devc->usb_mutex);

	if (ret != LIBUSB_SUCCESS) {
		sr_err("USB read failed: %s (endpoint 0x%02x, %zu bytes)", 
		       libusb_error_name(ret), LABJACK_USB_ENDPOINT_IN, length);
		return SR_ERR;
	}

	if (transferred != (int)length) {
		sr_err("USB read incomplete: %d/%zu bytes", transferred, length);
		return SR_ERR;
	}

	sr_spew("USB read successful: %d bytes", transferred);
	return SR_OK;
}

/**
 * Send a command packet to LabJack U12 and receive response.
 * 
 * @param sdi Device instance
 * @param request Request packet
 * @param response Response packet (can be NULL if no response expected)
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_send_command(const struct sr_dev_inst *sdi,
                                     const struct labjack_u12_packet *request,
                                     struct labjack_u12_packet *response)
{
	int ret;

	if (!sdi || !request)
		return SR_ERR_ARG;

	/* Send command */
	ret = labjack_u12_usb_write(sdi, request, LABJACK_USB_PACKET_SIZE);
	if (ret != SR_OK)
		return ret;

	/* Read response if requested */
	if (response) {
		ret = labjack_u12_usb_read(sdi, response, LABJACK_USB_PACKET_SIZE);
		if (ret != SR_OK)
			return ret;

		/* Verify command echo */
		if (response->command != request->command) {
			sr_err("Command echo mismatch: sent 0x%02x, got 0x%02x",
			       request->command, response->command);
			return SR_ERR;
		}
	}

	return SR_OK;
}

/**
 * Convert raw ADC value to voltage.
 * 
 * @param raw_value 12-bit ADC value
 * @param range Voltage range setting
 * @return Voltage value
 */
SR_PRIV float labjack_u12_raw_to_voltage(uint16_t raw_value, uint8_t range)
{
	float max_voltage;
	
	/* Determine voltage range */
	switch (range) {
	case LABJACK_AI_RANGE_10V:
		max_voltage = 10.0;
		break;
	case LABJACK_AI_RANGE_5V:
		max_voltage = 5.0;
		break;
	case LABJACK_AI_RANGE_2V:
		max_voltage = 2.0;
		break;
	case LABJACK_AI_RANGE_1V:
		max_voltage = 1.0;
		break;
	default:
		max_voltage = 10.0;
		break;
	}
	
	/* Convert 12-bit unsigned to signed voltage */
	/* LabJack U12 uses bipolar encoding: 0 = -max_voltage, 4095 = +max_voltage */
	return ((float)raw_value / (LABJACK_AI_RESOLUTION_12BIT - 1)) * (2.0 * max_voltage) - max_voltage;
}

/**
 * Convert voltage to raw DAC value.
 * 
 * @param voltage Voltage value (0-5V)
 * @return 12-bit DAC value
 */
SR_PRIV uint16_t labjack_u12_voltage_to_raw(float voltage)
{
	if (voltage < 0.0)
		voltage = 0.0;
	if (voltage > LABJACK_AO_MAX_VOLTAGE)
		voltage = LABJACK_AO_MAX_VOLTAGE;
	
	return (uint16_t)((voltage / LABJACK_AO_MAX_VOLTAGE) * (LABJACK_AO_RESOLUTION_12BIT - 1));
}

/**
 * Read an analog input channel.
 * 
 * @param sdi Device instance
 * @param channel AI channel number (0-7)
 * @param voltage Pointer to store voltage reading
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_read_ai_channel(const struct sr_dev_inst *sdi, 
                                        int channel, float *voltage)
{
	struct dev_context *devc;
	struct labjack_u12_ai_request request;
	struct labjack_u12_ai_response response;
	int ret;

	if (!sdi || !sdi->priv || !voltage || channel < 0 || channel > 7)
		return SR_ERR_ARG;

	devc = sdi->priv;

	/* Prepare AI request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_AI_SAMPLE;
	request.channel = channel;
	request.mode = (devc->ai_mode == AI_MODE_DIFFERENTIAL) ? 
	               LABJACK_AI_DIFFERENTIAL : LABJACK_AI_SINGLE_ENDED;
	request.range = devc->ai_range[channel]; /* Use per-channel range */

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	/* Check for errors */
	if (response.status != 0) {
		sr_err("AI read error on channel %d: status 0x%02x", channel, response.status);
		return SR_ERR;
	}

	/* Convert raw value to voltage */
	*voltage = labjack_u12_raw_to_voltage(response.raw_value, request.range);

	sr_spew("AI%d: raw=0x%04x, voltage=%.3fV, range=%d", 
	        channel, response.raw_value, *voltage, request.range);

	return SR_OK;
}

/**
 * Write an analog output channel.
 * 
 * @param sdi Device instance
 * @param channel AO channel number (0-1)
 * @param voltage Voltage to set (0-5V)
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_write_ao_channel(const struct sr_dev_inst *sdi,
                                         int channel, float voltage)
{
	struct labjack_u12_ao_request request;
	struct labjack_u12_packet response;
	int ret;

	if (!sdi || channel < 0 || channel > 1)
		return SR_ERR_ARG;

	if (voltage < 0.0 || voltage > LABJACK_AO_MAX_VOLTAGE) {
		sr_err("AO voltage out of range: %.3fV (must be 0-5V)", voltage);
		return SR_ERR_ARG;
	}

	/* Prepare AO request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_AO_UPDATE;
	request.channel = channel;
	request.raw_value = labjack_u12_voltage_to_raw(voltage);

	/* Send command */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              &response);
	if (ret != SR_OK)
		return ret;

	sr_spew("AO%d: voltage=%.3fV, raw=0x%04x", channel, voltage, request.raw_value);

	return SR_OK;
}

/**
 * Read digital I/O state from LabJack U12.
 * 
 * @param sdi Device instance
 * @param io_state Pointer to store IO0-IO3 state (bits 0-3)
 * @param d_state Pointer to store D0-D15 state (bits 0-15)
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_read_digital_io(const struct sr_dev_inst *sdi,
                                        uint32_t *io_state, uint32_t *d_state)
{
	struct labjack_u12_digital_io_request request;
	struct labjack_u12_digital_io_response response;
	int ret;

	if (!sdi || !io_state || !d_state)
		return SR_ERR_ARG;

	/* Prepare digital I/O read request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_DIGITAL_IO;
	/* Set all as inputs for reading */
	request.io_direction = 0x00;  /* All inputs */
	request.d_direction = 0x0000; /* All inputs */

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	*io_state = response.io_state & 0x0F;  /* Only bits 0-3 */
	*d_state = response.d_state & 0xFFFF;  /* All 16 bits */

	sr_spew("Digital I/O read: IO=0x%02x, D=0x%04x", *io_state, *d_state);

	return SR_OK;
}

/**
 * Write digital I/O state to LabJack U12.
 * 
 * @param sdi Device instance
 * @param io_direction IO0-IO3 direction (1=output, 0=input)
 * @param io_state IO0-IO3 output state
 * @param d_direction D0-D15 direction (1=output, 0=input)
 * @param d_state D0-D15 output state
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_write_digital_io(const struct sr_dev_inst *sdi,
                                         uint32_t io_direction, uint32_t io_state,
                                         uint32_t d_direction, uint32_t d_state)
{
	struct dev_context *devc;
	struct labjack_u12_digital_io_request request;
	struct labjack_u12_digital_io_response response;
	int ret;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;

	/* Prepare digital I/O write request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_DIGITAL_IO;
	request.io_direction = io_direction & 0x0F;
	request.io_state = io_state & 0x0F;
	request.d_direction = d_direction & 0xFFFF;
	request.d_state = d_state & 0xFFFF;

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	/* Update device context with current settings */
	for (int i = 0; i < 4; i++) {
		if (io_direction & (1 << i)) {
			devc->io_mode[i] = (io_state & (1 << i)) ? IO_MODE_OUTPUT_HIGH : IO_MODE_OUTPUT_LOW;
		} else {
			devc->io_mode[i] = IO_MODE_INPUT;
		}
	}

	for (int i = 0; i < 16; i++) {
		if (d_direction & (1 << i)) {
			devc->d_mode[i] = (d_state & (1 << i)) ? D_MODE_OUTPUT_HIGH : D_MODE_OUTPUT_LOW;
		} else {
			devc->d_mode[i] = D_MODE_INPUT;
		}
	}

	sr_spew("Digital I/O write: IO_dir=0x%02x, IO_state=0x%02x, D_dir=0x%04x, D_state=0x%04x",
	        io_direction, io_state, d_direction, d_state);

	return SR_OK;
}

/**
 * Read counter value from LabJack U12.
 * 
 * @param sdi Device instance
 * @param count Pointer to store counter value
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_read_counter(const struct sr_dev_inst *sdi, uint32_t *count)
{
	struct dev_context *devc;
	struct labjack_u12_counter_request request;
	struct labjack_u12_counter_response response;
	int ret;

	if (!sdi || !sdi->priv || !count)
		return SR_ERR_ARG;

	devc = sdi->priv;

	/* Prepare counter read request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_COUNTER;
	request.operation = LABJACK_COUNTER_READ;

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	*count = response.count;
	devc->counter_value = *count;
	devc->counter_timestamp = g_get_monotonic_time();

	sr_spew("Counter read: %u", *count);

	return SR_OK;
}

/**
 * Reset counter on LabJack U12.
 * 
 * @param sdi Device instance
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_reset_counter(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct labjack_u12_counter_request request;
	struct labjack_u12_counter_response response;
	int ret;

	if (!sdi || !sdi->priv)
		return SR_ERR_ARG;

	devc = sdi->priv;

	/* Prepare counter reset request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_COUNTER;
	request.operation = LABJACK_COUNTER_RESET;

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	devc->counter_value = 0;
	devc->counter_timestamp = g_get_monotonic_time();

	sr_info("Counter reset");

	return SR_OK;
}

/**
 * Perform bulk I/O operation (read multiple AI channels + digital I/O).
 * This is more efficient than individual operations.
 * 
 * @param sdi Device instance
 * @param ai_channels Bitmask of AI channels to read (bits 0-7)
 * @param ai_voltages Array to store AI voltage readings
 * @param io_direction IO direction bits
 * @param io_state IO output state
 * @param io_input Pointer to store IO input state
 * @param d_direction D direction bits
 * @param d_state D output state
 * @param d_input Pointer to store D input state
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_bulk_io(const struct sr_dev_inst *sdi,
                               uint8_t ai_channels, float *ai_voltages,
                               uint32_t io_direction, uint32_t io_state, uint32_t *io_input,
                               uint32_t d_direction, uint32_t d_state, uint32_t *d_input)
{
	struct labjack_u12_bulk_io_request request;
	struct labjack_u12_bulk_io_response response;
	int ret, ai_count = 0;

	if (!sdi)
		return SR_ERR_ARG;

	/* Prepare bulk I/O request */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_BULK_IO;
	request.ai_channels = ai_channels;
	request.io_direction = io_direction & 0x0F;
	request.io_state = io_state & 0x0F;
	request.d_direction = d_direction & 0xFFFF;

	/* Send command and get response */
	ret = labjack_u12_send_command(sdi, (struct labjack_u12_packet *)&request,
	                              (struct labjack_u12_packet *)&response);
	if (ret != SR_OK)
		return ret;

	/* Extract AI readings */
	if (ai_voltages) {
		for (int i = 0; i < 8 && ai_count < 4; i++) {
			if (ai_channels & (1 << i)) {
				ai_voltages[i] = labjack_u12_raw_to_voltage(response.ai_values[ai_count], 
				                                           LABJACK_AI_RANGE_10V);
				ai_count++;
			}
		}
	}

	/* Extract digital I/O states */
	if (io_input)
		*io_input = response.io_state & 0x0F;
	if (d_input)
		*d_input = response.d_state & 0xFFFF;

	sr_spew("Bulk I/O: AI_mask=0x%02x, IO_in=0x%02x, D_in=0x%04x", 
	        ai_channels, response.io_state, response.d_state);

	return SR_OK;
}

/**
 * Manually unbind HID driver from LabJack U12 device.
 * This is needed when udev rules don't work (e.g., in WSL2).
 * 
 * @param bus USB bus number
 * @param address USB device address
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_unbind_hid_driver(int bus, int address)
{
	char device_path[256];
	char unbind_path[256];
	FILE *unbind_file;
	int ret = SR_OK;

	sr_info("Attempting to unbind HID driver from LabJack U12 (bus %d, address %d)", bus, address);

	/* Try multiple interface path formats for different systems */
	const char *path_formats[] = {
		"%d-%d:1.0",     /* Standard format */
		"1-%d:1.0",      /* WSL2 format */
		"%d-1:1.0",      /* Alternative format */
		"1-1:1.0"        /* Fixed format for single device */
	};

	/* Try to unbind from usbhid driver */
	snprintf(unbind_path, sizeof(unbind_path), "/sys/bus/usb/drivers/usbhid/unbind");
	
	for (size_t i = 0; i < sizeof(path_formats) / sizeof(path_formats[0]); i++) {
		snprintf(device_path, sizeof(device_path), path_formats[i], bus, address);
		
		unbind_file = fopen(unbind_path, "w");
		if (unbind_file) {
			if (fprintf(unbind_file, "%s\n", device_path) > 0) {
				sr_info("Successfully unbound HID driver using path %s", device_path);
				fclose(unbind_file);
				break;
			}
			fclose(unbind_file);
		}
		sr_spew("Failed to unbind using path %s", device_path);
	}

	/* Try to unbind from hid-generic driver by searching for the device */
	if (access("/sys/bus/hid/devices", F_OK) == 0) {
		char cmd[512];
		snprintf(cmd, sizeof(cmd), 
		         "find /sys/bus/hid/devices -name '*' -exec grep -l 'HID_ID=0003:00000CD5:00000001' {}/uevent \\; 2>/dev/null | head -1");
		
		FILE *fp = popen(cmd, "r");
		if (fp) {
			char hid_path[256];
			if (fgets(hid_path, sizeof(hid_path), fp)) {
				/* Extract device name from path */
				char *last_slash = strrchr(hid_path, '/');
				if (last_slash) {
					*last_slash = '\0';
					last_slash = strrchr(hid_path, '/');
					if (last_slash) {
						char *hid_device = last_slash + 1;
						
						snprintf(unbind_path, sizeof(unbind_path), "/sys/bus/hid/drivers/hid-generic/unbind");
						unbind_file = fopen(unbind_path, "w");
						if (unbind_file) {
							if (fprintf(unbind_file, "%s\n", hid_device) > 0) {
								sr_info("Successfully unbound hid-generic driver from %s", hid_device);
							}
							fclose(unbind_file);
						}
					}
				}
			}
			pclose(fp);
		}
	}

	return ret;
}

/**
 * Reset the LabJack U12 device.
 * 
 * @param sdi Device instance
 * @return SR_OK on success, SR_ERR on failure
 */
SR_PRIV int labjack_u12_reset_device(const struct sr_dev_inst *sdi)
{
	struct labjack_u12_packet request;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;

	/* Prepare reset command */
	memset(&request, 0, sizeof(request));
	request.command = LABJACK_CMD_RESET;

	/* Send reset command (no response expected) */
	ret = labjack_u12_send_command(sdi, &request, NULL);
	if (ret != SR_OK)
		return ret;

	/* Give device time to reset */
	g_usleep(100 * 1000); /* 100ms */

	sr_info("LabJack U12 device reset");

	return SR_OK;
}

SR_PRIV int labjack_u12_receive_data(int fd, int revents, void *cb_data)
{
	const struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog analog;
	struct sr_datafeed_logic logic;
	struct sr_analog_encoding encoding;
	struct sr_analog_meaning meaning;
	struct sr_analog_spec spec;
	GSList *l;
	struct sr_channel *ch;
	float *analog_data;
	uint8_t *logic_data;
	int num_analog_channels = 0;
	int num_logic_channels = 0;
	int analog_index = 0;
	int logic_index = 0;
	float voltage;
	uint32_t io_state, d_state, counter_value;
	int ret;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	if (!sdi)
		return TRUE;

	devc = sdi->priv;
	if (!devc || !devc->acquisition_running)
		return TRUE;

	/* Check sample limit */
	if (devc->limit_samples > 0 && devc->num_samples >= devc->limit_samples) {
		sr_dev_acquisition_stop((struct sr_dev_inst *)sdi);
		return TRUE;
	}

	/* Count enabled channels */
	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;
		
		if (ch->type == SR_CHANNEL_ANALOG) {
			if (ch->index < 8 || strcmp(ch->name, "CNT") == 0) {
				num_analog_channels++;
			}
		} else if (ch->type == SR_CHANNEL_LOGIC) {
			num_logic_channels++;
		}
	}

	if (num_analog_channels == 0 && num_logic_channels == 0) {
		sr_dev_acquisition_stop((struct sr_dev_inst *)sdi);
		return TRUE;
	}

	/* Read digital I/O state if any digital channels are enabled */
	if (num_logic_channels > 0) {
		ret = labjack_u12_read_digital_io(sdi, &io_state, &d_state);
		if (ret != SR_OK) {
			sr_err("Failed to read digital I/O");
			io_state = 0;
			d_state = 0;
		}
	}

	/* Handle analog channels */
	if (num_analog_channels > 0) {
		analog_data = g_malloc(num_analog_channels * sizeof(float));

		/* Read all enabled AI channels and counter */
		for (l = sdi->channels; l; l = l->next) {
			ch = l->data;
			if (!ch->enabled || ch->type != SR_CHANNEL_ANALOG)
				continue;

			if (ch->index < 8) {
				/* AI channel */
				ret = labjack_u12_read_ai_channel(sdi, ch->index, &voltage);
				if (ret != SR_OK) {
					sr_err("Failed to read AI%d: %d", ch->index, ret);
					voltage = NAN;
				}
				analog_data[analog_index++] = voltage;
			} else if (strcmp(ch->name, "CNT") == 0) {
				/* Counter channel */
				ret = labjack_u12_read_counter(sdi, &counter_value);
				if (ret != SR_OK) {
					sr_err("Failed to read counter");
					counter_value = 0;
				}
				analog_data[analog_index++] = (float)counter_value;
			}
		}

		/* Send analog packet */
		sr_analog_init(&analog, &encoding, &meaning, &spec, 3);
		analog.data = analog_data;
		analog.num_samples = 1;
		
		/* Set up analog channels list */
		analog.meaning->channels = NULL;
		for (l = sdi->channels; l; l = l->next) {
			ch = l->data;
			if (ch->enabled && ch->type == SR_CHANNEL_ANALOG && 
			    (ch->index < 8 || strcmp(ch->name, "CNT") == 0)) {
				analog.meaning->channels = g_slist_append(analog.meaning->channels, ch);
			}
		}
		
		analog.meaning->mq = SR_MQ_VOLTAGE;
		analog.meaning->unit = SR_UNIT_VOLT;
		analog.meaning->mqflags = 0;

		packet.type = SR_DF_ANALOG;
		packet.payload = &analog;
		sr_session_send(sdi, &packet);

		g_slist_free(analog.meaning->channels);
		g_free(analog_data);
	}

	/* Handle logic channels */
	if (num_logic_channels > 0) {
		/* Calculate number of bytes needed for logic data */
		int logic_bytes = (num_logic_channels + 7) / 8;
		logic_data = g_malloc0(logic_bytes);

		/* Pack digital channel states into logic data */
		for (l = sdi->channels; l; l = l->next) {
			ch = l->data;
			if (!ch->enabled || ch->type != SR_CHANNEL_LOGIC)
				continue;

			gboolean bit_value = FALSE;
			
			if (ch->index >= 10 && ch->index < 14) {
				/* IO0-IO3 channels (indices 10-13) */
				int io_bit = ch->index - 10;
				bit_value = (io_state & (1 << io_bit)) ? TRUE : FALSE;
			} else if (ch->index >= 14 && ch->index < 30) {
				/* D0-D15 channels (indices 14-29) */
				int d_bit = ch->index - 14;
				bit_value = (d_state & (1 << d_bit)) ? TRUE : FALSE;
			}

			if (bit_value) {
				logic_data[logic_index / 8] |= (1 << (logic_index % 8));
			}
			logic_index++;
		}

		/* Send logic packet */
		logic.data = logic_data;
		logic.length = logic_bytes;
		logic.unitsize = logic_bytes;

		packet.type = SR_DF_LOGIC;
		packet.payload = &logic;
		sr_session_send(sdi, &packet);

		g_free(logic_data);
	}

	devc->num_samples++;

	return TRUE;
}
