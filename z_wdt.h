/*
 * Embedded Watchdog Framework
 */

#ifndef Z_WDT_H
#define Z_WDT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configuration */
#define WATCHDOG_MAX_CHANNELS 16

/* Callback function type */
typedef void (*watchdog_callback_t)(int channel_id, void *user_data);

/* Public API */
int z_wdt_init(void);
int z_wdt_add(uint32_t reload_period, watchdog_callback_t callback, void *user_data);
int z_wdt_delete(int channel_id);
int z_wdt_feed(int channel_id);
void z_wdt_suspend(void);
void z_wdt_resume(void);
void z_wdt_cleanup(void);

/* Platform internal API (called by platform layer) */
void z_wdt_process(void);

#ifdef __cplusplus
}
#endif

#endif // Z_WDT_H
