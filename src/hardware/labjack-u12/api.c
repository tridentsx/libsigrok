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
#include "protocol.h"
#include <libusb.h>
#include "libsigrok/libsigrok.h"
#include "libsigrok-internal.h"

static struct sr_dev_driver labjack_u12_driver_info;
GSList *dev_scan(struct sr_dev_driver *di, GSList *options);


static const uint32_t scanopts[] = {
    SR_CONF_CONN,
    0,
};

static const uint32_t drvopts[] = {
   // SR_CONF_SCAN,           // Allows detection via --scan
    SR_CONF_CONN,           // Connection information
    SR_CONF_SAMPLERATE,     // Sampling rate setting
    SR_CONF_VOLTAGE,        // Hypothetical voltage range setting
    SR_CONF_LIMIT_SAMPLES,  // Sample limit
    0
};


SR_PRIV GSList *dev_scan(struct sr_dev_driver *di, GSList *options)
{
    struct sr_dev_inst *sdi;
    struct sr_usb_dev_inst *usb;
    libusb_context *usb_ctx;
    libusb_device **devlist;
    libusb_device *dev;
    struct libusb_device_descriptor desc;
    ssize_t num_devs;
    int i, r;
    GSList *found_devs = NULL;

    if ((r = libusb_init(&usb_ctx)) != 0) {
        sr_err("libusb_init failed: %s.", libusb_error_name(r));
        return NULL;
    }

    num_devs = libusb_get_device_list(usb_ctx, &devlist);
    for (i = 0; i < num_devs; i++) {
        dev = devlist[i];
        r = libusb_get_device_descriptor(dev, &desc);
        if (r != 0)
            continue;

        if (desc.idVendor == LABJACK_VENDOR_ID && desc.idProduct == LABJACK_PRODUCT_ID) {
            sdi = g_malloc0(sizeof(struct sr_dev_inst));
            sdi->status = SR_ST_INACTIVE;
            sdi->driver = di;
            sdi->vendor = g_strdup("LabJack");
            sdi->model = g_strdup("U12");

            usb = g_malloc0(sizeof(struct sr_usb_dev_inst));
            usb->address = libusb_get_device_address(dev);
            usb->bus = libusb_get_bus_number(dev);
            sdi->conn = usb;
            sdi->inst_type = SR_INST_USB;

            found_devs = g_slist_append(found_devs, sdi);
        }
    }

    libusb_free_device_list(devlist, 1);
    libusb_exit(usb_ctx);

    return found_devs;
}



static int dev_open(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO: get handle from sdi->conn and open it. */

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO: get handle from sdi->conn and close it. */

	return SR_OK;
}


static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg)
{
	struct dev_context *ctx = sdi->priv;

	if (!ctx || !data)
		return SR_ERR_ARG;

	if (key == SR_CONF_DEVICE_OPTIONS) {
		/* Not used in get; handled in list */
		return SR_ERR_NA;
	}

	if (key == SR_CONF_DEVICE_MODE) {
		const char *mode_str = (ctx->ai_mode == AI_MODE_DIFFERENTIAL) ? "differential" : "single-ended";
		*data = g_variant_new_string(mode_str);
		return SR_OK;
	}

	/* AO outputs */
	if (key == SR_CONF_VOLTAGE) {
		*data = g_variant_new_double(ctx->ao_voltage[0]); // Example for AO0
		return SR_OK;
	}

	/* Add more cases as needed... */

	return SR_ERR_NA;
}

static int config_set(uint32_t key, GVariant *data,
	struct sr_dev_inst *sdi,
	struct sr_channel_group *cg)
{
	struct dev_context *ctx = sdi->priv;

	if (!ctx || !data)
		return SR_ERR_ARG;

	if (key == SR_CONF_DEVICE_MODE) {
		const char *mode_str = g_variant_get_string(data, NULL);
		if (g_strcmp0(mode_str, "differential") == 0) {
			ctx->ai_mode = AI_MODE_DIFFERENTIAL;
		} else if (g_strcmp0(mode_str, "single-ended") == 0) {
			ctx->ai_mode = AI_MODE_SINGLE_ENDED;
		} else {
			return SR_ERR_ARG;
		}
		return SR_OK;
	}

	if (key == SR_CONF_VOLTAGE) {
		double voltage = g_variant_get_double(data);
		ctx->ao_voltage[0] = voltage; // AO0 example
		return SR_OK;
	}

	/* Add more cases for IOx and Dx mode... */

	return SR_ERR_NA;
}


static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi,
	const struct sr_channel_group *cg)
{
	if (key == SR_CONF_DEVICE_OPTIONS) {
		const uint32_t opts[] = {
		SR_CONF_DEVICE_MODE,
		SR_CONF_VOLTAGE,
		/* Add more exposed config keys here */
		};
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				opts, G_N_ELEMENTS(opts), sizeof(uint32_t));
		return SR_OK;
	}

	if (key == SR_CONF_DEVICE_MODE) {
		const char *modes[] = {"single-ended", "differential"};
		*data = g_variant_new_strv(modes, G_N_ELEMENTS(modes));
		return SR_OK;
	}

	if (key == SR_CONF_VOLTAGE) {
		/* AO0/AO1 output is between 0–5V */
		double range[] = {0.0, 5.0};
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_DOUBLE,
							range, 2, sizeof(double));
		return SR_OK;
	}

	return SR_ERR_NA;
}


static int dev_acquisition_start(const struct sr_dev_inst *sdi)
{
	/* TODO: configure hardware, reset acquisition state, set up
	 * callbacks and send header packet. */

	(void)sdi;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi)
{
	/* TODO: stop acquisition. */

	(void)sdi;

	return SR_OK;
}

static struct sr_dev_driver labjack_u12_driver_info = {
	.name = "labjack-u12",
	.longname = "LabJack U12",
	.api_version = 1,
	.init = std_init,
	.cleanup = std_cleanup,
	.scan = dev_scan,
	.dev_list = std_dev_list,
	.dev_clear = std_dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
SR_REGISTER_DEV_DRIVER(labjack_u12_driver_info);
