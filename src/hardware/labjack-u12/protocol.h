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
};

SR_PRIV int labjack_u12_receive_data(int fd, int revents, void *cb_data);

#endif
