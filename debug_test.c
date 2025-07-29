/*
 * Debug test for LabJack U12 driver to isolate crash
 */

#include <stdio.h>
#include <libsigrok/libsigrok.h>

int main(void)
{
    struct sr_context *ctx;
    struct sr_dev_driver **drivers;
    struct sr_dev_driver *driver = NULL;
    GSList *devices = NULL;
    int i, ret;

    printf("Initializing libsigrok...\n");
    if ((ret = sr_init(&ctx)) != SR_OK) {
        printf("Failed to initialize libsigrok: %d\n", ret);
        return 1;
    }

    printf("Finding LabJack U12 driver...\n");
    drivers = sr_driver_list(ctx);
    for (i = 0; drivers[i]; i++) {
        if (strcmp(drivers[i]->name, "labjack-u12") == 0) {
            driver = drivers[i];
            printf("Found driver: %s\n", driver->longname);
            break;
        }
    }

    if (!driver) {
        printf("LabJack U12 driver not found\n");
        sr_exit(ctx);
        return 1;
    }

    printf("Initializing driver...\n");
    if (sr_driver_init(ctx, driver) != SR_OK) {
        printf("Failed to initialize driver\n");
        sr_exit(ctx);
        return 1;
    }

    printf("Scanning for devices...\n");
    fflush(stdout);
    
    devices = sr_driver_scan(driver, NULL);
    
    printf("Scan completed. Found %d devices\n", g_slist_length(devices));

    if (devices) {
        printf("Device scan successful!\n");
        struct sr_dev_inst *sdi = devices->data;
        
        printf("Attempting to open device...\n");
        fflush(stdout);
        
        ret = sr_dev_open(sdi);
        if (ret == SR_OK) {
            printf("Device opened successfully!\n");
            sr_dev_close(sdi);
            printf("Device closed successfully!\n");
        } else {
            printf("Failed to open device: %d\n", ret);
        }
        
        g_slist_free(devices);
    } else {
        printf("No devices found\n");
    }

    printf("Cleaning up...\n");
    sr_exit(ctx);
    printf("Test completed successfully!\n");
    
    return 0;
}
