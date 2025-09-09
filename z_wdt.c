/*
 * Embedded Watchdog Framework Implementation
 */

#include "z_wdt.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Internal data structures */
struct watchdog_channel {
    uint32_t reload_period;        // Period in milliseconds
    int64_t timeout_abs_ticks;     // Absolute timeout in ticks
    void *user_data;               // User data for callback
    watchdog_callback_t callback;  // Callback function
    bool active;                   // Channel active flag
};

struct watchdog_context {
    struct watchdog_channel channels[WATCHDOG_MAX_CHANNELS];
    int64_t current_ticks;         // Current system ticks
    int next_timeout_channel;      // Next channel to timeout
    int64_t next_timeout_ticks;    // Next timeout in ticks
    bool initialized;              // Initialization flag
    bool timer_running;            // Timer running flag
};

/* Global watchdog context */
static struct watchdog_context g_watchdog_ctx = {0};

/* Platform abstraction functions (must be implemented by platform layer) */
extern int64_t watchdog_get_ticks(void);
extern void watchdog_timer_start(int64_t timeout_ticks);
extern void watchdog_timer_stop(void);
extern void watchdog_log(const char *level, const char *format, ...);
extern int watchdog_os_init(void);
extern void watchdog_os_cleanup(void);
extern void watchdog_mutex_lock(void);
extern void watchdog_mutex_unlock(void);

/* Internal utility functions */
static int64_t watchdog_ms_to_ticks(uint32_t ms);
static int watchdog_get_next_timeout_channel(void);
static void watchdog_schedule_next_timeout(void);

// Initialize watchdog system
int z_wdt_init(void) {
    if (g_watchdog_ctx.initialized) {
        watchdog_log("WARN", "Watchdog already initialized");
        return 0;
    }
    
    // Initialize context
    memset(&g_watchdog_ctx, 0, sizeof(g_watchdog_ctx));
    g_watchdog_ctx.next_timeout_channel = -1;
    g_watchdog_ctx.next_timeout_ticks = INT64_MAX;
    g_watchdog_ctx.initialized = true;
    g_watchdog_ctx.timer_running = true;
    
    // Initialize OS layer
    if (watchdog_os_init() != 0) {
        return -1;
    }
    
    watchdog_log("INFO", "Watchdog initialized successfully");
    return 0;
}

// Add a watchdog channel
int z_wdt_add(uint32_t reload_period, watchdog_callback_t callback, void *user_data) {
    if (!g_watchdog_ctx.initialized) {
        watchdog_log("ERROR", "Watchdog not initialized");
        return -1;
    }
    
    if (reload_period == 0) {
        watchdog_log("ERROR", "Invalid reload period: 0");
        return -1;
    }
    
    watchdog_mutex_lock();
    
    // Find unused channel
    for (int id = 0; id < WATCHDOG_MAX_CHANNELS; id++) {
        if (!g_watchdog_ctx.channels[id].active) {
            g_watchdog_ctx.channels[id].reload_period = reload_period;
            g_watchdog_ctx.channels[id].user_data = user_data;
            g_watchdog_ctx.channels[id].timeout_abs_ticks = INT64_MAX;
            g_watchdog_ctx.channels[id].callback = callback;
            g_watchdog_ctx.channels[id].active = true;
            
            // Feed the channel immediately
            z_wdt_feed(id);
            
            watchdog_mutex_unlock();
            
            watchdog_log("INFO", "Added watchdog channel %d with period %ums", id, reload_period);
            return id;
        }
    }
    
    watchdog_mutex_unlock();
    watchdog_log("ERROR", "No available watchdog channels");
    return -1;
}

// Delete a watchdog channel
int z_wdt_delete(int channel_id) {
    if (!g_watchdog_ctx.initialized) {
        watchdog_log("ERROR", "Watchdog not initialized");
        return -1;
    }
    
    if (channel_id < 0 || channel_id >= WATCHDOG_MAX_CHANNELS) {
        watchdog_log("ERROR", "Invalid channel ID: %d", channel_id);
        return -1;
    }
    
    watchdog_mutex_lock();
    
    if (g_watchdog_ctx.channels[channel_id].active) {
        g_watchdog_ctx.channels[channel_id].active = false;
        g_watchdog_ctx.channels[channel_id].reload_period = 0;
        g_watchdog_ctx.channels[channel_id].callback = NULL;
        g_watchdog_ctx.channels[channel_id].user_data = NULL;
        
        // Reschedule next timeout
        watchdog_schedule_next_timeout();
        
        watchdog_mutex_unlock();
        
        watchdog_log("INFO", "Deleted watchdog channel %d", channel_id);
        return 0;
    }
    
    watchdog_mutex_unlock();
    watchdog_log("WARN", "Channel %d not active", channel_id);
    return -1;
}

// Feed a watchdog channel
int z_wdt_feed(int channel_id) {
    if (!g_watchdog_ctx.initialized) {
        return -1;
    }
    
    if (channel_id < 0 || channel_id >= WATCHDOG_MAX_CHANNELS) {
        return -1;
    }
    
    if (!g_watchdog_ctx.channels[channel_id].active) {
        return -1;
    }
    
    int64_t current_ticks = watchdog_get_ticks();
    
    // Update timeout for this channel
    g_watchdog_ctx.channels[channel_id].timeout_abs_ticks = 
        current_ticks + watchdog_ms_to_ticks(g_watchdog_ctx.channels[channel_id].reload_period);
    
    // Reschedule next timeout
    watchdog_schedule_next_timeout();
    
    return 0;
}

// Suspend watchdog (for power management)
void z_wdt_suspend(void) {
    if (!g_watchdog_ctx.initialized) {
        return;
    }
    
    watchdog_mutex_lock();
    g_watchdog_ctx.timer_running = false;
    watchdog_mutex_unlock();
    
    watchdog_log("INFO", "Watchdog suspended");
}

// Resume watchdog (for power management)
void z_wdt_resume(void) {
    if (!g_watchdog_ctx.initialized) {
        return;
    }
    
    watchdog_mutex_lock();
    
    // Feed all active channels
    int64_t current_ticks = watchdog_get_ticks();
    for (int id = 0; id < WATCHDOG_MAX_CHANNELS; id++) {
        if (g_watchdog_ctx.channels[id].active) {
            g_watchdog_ctx.channels[id].timeout_abs_ticks = 
                current_ticks + watchdog_ms_to_ticks(g_watchdog_ctx.channels[id].reload_period);
        }
    }
    
    g_watchdog_ctx.timer_running = true;
    watchdog_schedule_next_timeout();
    
    watchdog_mutex_unlock();
    
    watchdog_log("INFO", "Watchdog resumed");
}

// Process watchdog (called by timer thread)
void z_wdt_process(void) {
    if (!g_watchdog_ctx.initialized || !g_watchdog_ctx.timer_running) {
        return;
    }
    
    int64_t current_ticks = watchdog_get_ticks();
    
    // Check for timeouts
    for (int id = 0; id < WATCHDOG_MAX_CHANNELS; id++) {
        if (g_watchdog_ctx.channels[id].active && 
            g_watchdog_ctx.channels[id].timeout_abs_ticks <= current_ticks) {
            
            watchdog_log("ERROR", "Watchdog channel %d timeout!", id);
            
            if (g_watchdog_ctx.channels[id].callback) {
                g_watchdog_ctx.channels[id].callback(id, g_watchdog_ctx.channels[id].user_data);
            } else {
                watchdog_log("FATAL", "No callback for channel %d, system will exit", id);
                exit(1);
            }
            
            // Deactivate the channel after timeout
            g_watchdog_ctx.channels[id].active = false;
        }
    }
    
    // Reschedule next timeout
    watchdog_schedule_next_timeout();
}

// Convert milliseconds to ticks
static int64_t watchdog_ms_to_ticks(uint32_t ms) {
    return (int64_t)ms;
}

// Get next timeout channel
static int watchdog_get_next_timeout_channel(void) {
    int next_channel = -1;
    int64_t next_timeout = INT64_MAX;
    
    for (int id = 0; id < WATCHDOG_MAX_CHANNELS; id++) {
        if (g_watchdog_ctx.channels[id].active && 
            g_watchdog_ctx.channels[id].timeout_abs_ticks < next_timeout) {
            next_channel = id;
            next_timeout = g_watchdog_ctx.channels[id].timeout_abs_ticks;
        }
    }
    
    return next_channel;
}

// Schedule next timeout
static void watchdog_schedule_next_timeout(void) {
    int next_channel = watchdog_get_next_timeout_channel();
    
    if (next_channel >= 0) {
        g_watchdog_ctx.next_timeout_channel = next_channel;
        g_watchdog_ctx.next_timeout_ticks = g_watchdog_ctx.channels[next_channel].timeout_abs_ticks;
    } else {
        g_watchdog_ctx.next_timeout_channel = -1;
        g_watchdog_ctx.next_timeout_ticks = INT64_MAX;
    }
}


// Cleanup function
void z_wdt_cleanup(void) {
    if (g_watchdog_ctx.initialized) {
        watchdog_os_cleanup();
        g_watchdog_ctx.initialized = false;
        watchdog_log("INFO", "Watchdog cleaned up");
    }
}