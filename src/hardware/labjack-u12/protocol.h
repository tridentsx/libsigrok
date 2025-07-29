#ifndef LIBSIGROK_HARDWARE_LABJACK_U12_PROTOCOL_H
#define LIBSIGROK_HARDWARE_LABJACK_U12_PROTOCOL_H

#include <stdint.h>
#include <glib.h>
#include <stdbool.h>
#include <libsigrok/libsigrok.h>
#include "libsigrok-internal.h"

#define LOG_PREFIX "labjack-u12"

#define LABJACK_VENDOR_ID  0x0cd5
#define LABJACK_PRODUCT_ID 0x0001

/* USB communication constants */
#define LABJACK_USB_INTERFACE     0
#define LABJACK_USB_TIMEOUT_MS    1000
#define LABJACK_USB_ENDPOINT_OUT  0x02  /* EP 2 OUT from descriptor */
#define LABJACK_USB_ENDPOINT_IN   0x81  /* EP 1 IN from descriptor */

/* LabJack U12 USB packet structure */
#define LABJACK_USB_PACKET_SIZE   8

/* LabJack U12 command constants */
#define LABJACK_CMD_RESET         0x99
#define LABJACK_CMD_AI_SAMPLE     0x01
#define LABJACK_CMD_AO_UPDATE     0x02
#define LABJACK_CMD_DIGITAL_IO    0x03
#define LABJACK_CMD_COUNTER       0x04
#define LABJACK_CMD_WATCHDOG      0x05
#define LABJACK_CMD_READ_RAM      0x06
#define LABJACK_CMD_WRITE_RAM     0x07
#define LABJACK_CMD_READ_ROM      0x08
#define LABJACK_CMD_BULK_IO       0x09  /* Combined I/O operations */

/* AI sampling modes */
#define LABJACK_AI_SINGLE_ENDED   0x00
#define LABJACK_AI_DIFFERENTIAL   0x01

/* Digital I/O modes */
#define LABJACK_IO_INPUT          0x00
#define LABJACK_IO_OUTPUT_LOW     0x01
#define LABJACK_IO_OUTPUT_HIGH    0x02

/* Counter modes */
#define LABJACK_COUNTER_RESET     0x01
#define LABJACK_COUNTER_READ      0x02

/* Voltage ranges and conversion */
#define LABJACK_AI_RANGE_10V      0x00  /* ±10V */
#define LABJACK_AI_RANGE_5V       0x01  /* ±5V */
#define LABJACK_AI_RANGE_2V       0x02  /* ±2V */
#define LABJACK_AI_RANGE_1V       0x03  /* ±1V */

#define LABJACK_AI_RESOLUTION_12BIT  4096
#define LABJACK_AI_MAX_VOLTAGE       10.0
#define LABJACK_AO_MAX_VOLTAGE       5.0
#define LABJACK_AO_RESOLUTION_12BIT  4096

/* Config keys as strings */
#define CONFIG_KEY_AI_MODE        "ai-mode"
#define CONFIG_KEY_AI_ENABLED     "ai-enabled"
#define CONFIG_KEY_AI_DIFF_ENABLED "ai-diff-enabled"
#define CONFIG_KEY_AO_VOLTAGE     "ao-voltage"
#define CONFIG_KEY_IO_MODE        "io-mode"
#define CONFIG_KEY_D_MODE         "d-mode"
#define CONFIG_KEY_COUNTER_VALUE  "counter"
#define CONFIG_KEY_IS_OPEN        "is-open"

/* Allowed values for analog input modes */
#define AI_MODE_SINGLE_ENDED_STR   "single-ended"
#define AI_MODE_DIFFERENTIAL_STR   "differential"

/* Allowed values for IO/D modes */
#define IO_MODE_INPUT_STR          "input"
#define IO_MODE_OUTPUT_LOW_STR     "output-low"
#define IO_MODE_OUTPUT_HIGH_STR    "output-high"

#define D_MODE_INPUT_STR           "input"
#define D_MODE_OUTPUT_LOW_STR      "output-low"
#define D_MODE_OUTPUT_HIGH_STR     "output-high"

/* USB packet structures */
struct labjack_u12_packet {
	uint8_t command;
	uint8_t data[7];
};

struct labjack_u12_ai_request {
	uint8_t command;        /* LABJACK_CMD_AI_SAMPLE */
	uint8_t channel;        /* AI channel number (0-7) */
	uint8_t mode;          /* Single-ended or differential */
	uint8_t range;         /* Voltage range */
	uint8_t reserved[4];   /* Padding to 8 bytes */
};

struct labjack_u12_ai_response {
	uint8_t command;       /* Echo of command */
	uint8_t channel;       /* Echo of channel */
	uint16_t raw_value;    /* 12-bit ADC value */
	uint8_t status;        /* Status/error code */
	uint8_t reserved[3];   /* Padding */
};

struct labjack_u12_ao_request {
	uint8_t command;       /* LABJACK_CMD_AO_UPDATE */
	uint8_t channel;       /* AO channel (0-1) */
	uint16_t raw_value;    /* 12-bit DAC value */
	uint8_t reserved[4];   /* Padding */
};

struct labjack_u12_digital_io_request {
	uint8_t command;       /* LABJACK_CMD_DIGITAL_IO */
	uint8_t io_direction;  /* IO0-IO3 direction bits (1=output, 0=input) */
	uint8_t io_state;      /* IO0-IO3 output state bits */
	uint16_t d_direction;  /* D0-D15 direction bits */
	uint16_t d_state;      /* D0-D15 output state bits */
	uint8_t reserved;      /* Padding */
};

struct labjack_u12_digital_io_response {
	uint8_t command;       /* Echo of command */
	uint8_t io_state;      /* IO0-IO3 input state */
	uint16_t d_state;      /* D0-D15 input state */
	uint8_t reserved[4];   /* Padding */
};

struct labjack_u12_counter_request {
	uint8_t command;       /* LABJACK_CMD_COUNTER */
	uint8_t operation;     /* RESET or READ */
	uint8_t reserved[6];   /* Padding */
};

struct labjack_u12_counter_response {
	uint8_t command;       /* Echo of command */
	uint8_t operation;     /* Echo of operation */
	uint32_t count;        /* Counter value */
	uint8_t reserved[2];   /* Padding */
};

struct labjack_u12_bulk_io_request {
	uint8_t command;       /* LABJACK_CMD_BULK_IO */
	uint8_t ai_channels;   /* Bitmask of AI channels to read */
	uint8_t io_direction;  /* IO direction bits */
	uint8_t io_state;      /* IO output state */
	uint16_t d_direction;  /* D direction bits */
	uint8_t reserved;      /* Padding */
};

struct labjack_u12_bulk_io_response {
	uint8_t command;       /* Echo of command */
	uint16_t ai_values[4]; /* AI readings (up to 4 channels) */
	uint8_t io_state;      /* IO input state */
	uint16_t d_state;      /* D input state */
	uint8_t reserved;      /* Padding */
};

struct dev_context {
	/* Analog input configuration */
	enum {
		AI_MODE_SINGLE_ENDED,
		AI_MODE_DIFFERENTIAL
	} ai_mode;

	/* Per-AI-channel enablement (used in single-ended mode) */
	bool ai_enabled[8]; /* AI0–AI7 */

	/* In differential mode, each pair is treated as one channel: AI0+AI1, AI2+AI3, ... */
	bool ai_diff_enabled[4]; /* AI0/1, AI2/3, AI4/5, AI6/7 */

	/* AI voltage ranges per channel */
	uint8_t ai_range[8]; /* Voltage range for each AI channel */

	/* Analog outputs (0–5V), AO0–AO1 */
	float ao_voltage[2]; /* AO0, AO1 */

	/* IO0–IO3 configuration and state */
	enum {
		IO_MODE_INPUT,
		IO_MODE_OUTPUT_LOW,
		IO_MODE_OUTPUT_HIGH
	} io_mode[4]; /* IO0–IO3 */

	/* D0–D15 (raw digital I/O on DB25) configuration */
	enum {
		D_MODE_INPUT,
		D_MODE_OUTPUT_LOW,
		D_MODE_OUTPUT_HIGH
	} d_mode[16]; /* D0–D15 */

	/* Counter (CNT) state */
	uint32_t counter_value;

	/* Timestamp of last CNT sample, if needed */
	uint64_t counter_timestamp;

	/* Additional device state */
	bool is_open;

	/* USB communication state */
	struct sr_usb_dev_inst *usb;
	GMutex usb_mutex;  /* Protect USB operations */
	
	/* Acquisition state */
	uint64_t limit_samples;
	uint64_t num_samples;
	gboolean acquisition_running;
};

/* Helper functions for channel management */
SR_PRIV bool labjack_u12_is_ai_channel_available(const struct dev_context *devc, int ai_index);
SR_PRIV int labjack_u12_get_differential_pair(int ai_index);
SR_PRIV bool labjack_u12_ai_channels_conflict(const struct dev_context *devc, int ai_index);

/* USB communication functions */
SR_PRIV int labjack_u12_usb_write(const struct sr_dev_inst *sdi, 
                                  const void *data, size_t length);
SR_PRIV int labjack_u12_usb_read(const struct sr_dev_inst *sdi, 
                                 void *data, size_t length);
SR_PRIV int labjack_u12_send_command(const struct sr_dev_inst *sdi,
                                     const struct labjack_u12_packet *request,
                                     struct labjack_u12_packet *response);

/* Hardware abstraction functions */
SR_PRIV int labjack_u12_read_ai_channel(const struct sr_dev_inst *sdi, 
                                        int channel, float *voltage);
SR_PRIV int labjack_u12_write_ao_channel(const struct sr_dev_inst *sdi,
                                         int channel, float voltage);
SR_PRIV int labjack_u12_read_digital_io(const struct sr_dev_inst *sdi,
                                        uint32_t *io_state, uint32_t *d_state);
SR_PRIV int labjack_u12_write_digital_io(const struct sr_dev_inst *sdi,
                                         uint32_t io_direction, uint32_t io_state,
                                         uint32_t d_direction, uint32_t d_state);
SR_PRIV int labjack_u12_read_counter(const struct sr_dev_inst *sdi, uint32_t *count);
SR_PRIV int labjack_u12_reset_counter(const struct sr_dev_inst *sdi);
SR_PRIV int labjack_u12_reset_device(const struct sr_dev_inst *sdi);
SR_PRIV int labjack_u12_bulk_io(const struct sr_dev_inst *sdi,
                               uint8_t ai_channels, float *ai_voltages,
                               uint32_t io_direction, uint32_t io_state, uint32_t *io_input,
                               uint32_t d_direction, uint32_t d_state, uint32_t *d_input);

/* Utility functions */
SR_PRIV float labjack_u12_raw_to_voltage(uint16_t raw_value, uint8_t range);
SR_PRIV uint16_t labjack_u12_voltage_to_raw(float voltage);
SR_PRIV int labjack_u12_unbind_hid_driver(int bus, int address);

SR_PRIV int labjack_u12_receive_data(int fd, int revents, void *cb_data);

#endif
