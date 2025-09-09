/*
 * Watchdog OS Abstraction Layer
 * Contains platform-specific system operations
 */

#include "z_wdt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define usleep(x) Sleep((x)/1000)
    #define sleep(x) Sleep((x)*1000)
#else
    #include <unistd.h>
    #include <sys/time.h>
    #include <pthread.h>
#endif

// Threading variables
#ifdef _WIN32
static HANDLE timer_thread;
static bool timer_thread_running = false;
static CRITICAL_SECTION watchdog_mutex;
#else
static pthread_t timer_thread;
static bool timer_thread_running = false;
static pthread_mutex_t watchdog_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

// Platform abstraction implementation
int64_t watchdog_get_ticks(void) {
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; // Convert to milliseconds
#endif
}

void watchdog_timer_start(int64_t timeout_ticks) {
    // Timer is handled by the timer thread
    (void)timeout_ticks;
}

void watchdog_timer_stop(void) {
    // Timer is handled by the timer thread
}

void watchdog_log(const char *level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("[%s] [%s] ", timestamp, level);
    vprintf(format, args);
    printf("\n");
    
    va_end(args);
}

// Timer thread function
#ifdef _WIN32
static DWORD WINAPI timer_thread_func(LPVOID arg) {
    (void)arg;
    
    while (timer_thread_running) {
        EnterCriticalSection(&watchdog_mutex);
        z_wdt_process();
        LeaveCriticalSection(&watchdog_mutex);
        
        // Sleep for 100ms
        Sleep(100);
    }
    
    return 0;
}
#else
static void* timer_thread_func(void *arg) {
    (void)arg;
    
    while (timer_thread_running) {
        pthread_mutex_lock(&watchdog_mutex);
        z_wdt_process();
        pthread_mutex_unlock(&watchdog_mutex);
        
        // Sleep for 100ms
        usleep(100000);
    }
    
    return NULL;
}
#endif

// OS-specific initialization
int watchdog_os_init(void) {
    // Initialize mutex
#ifdef _WIN32
    InitializeCriticalSection(&watchdog_mutex);
#endif
    
    // Start timer thread
    timer_thread_running = true;
#ifdef _WIN32
    timer_thread = CreateThread(NULL, 0, timer_thread_func, NULL, 0, NULL);
    if (timer_thread == NULL) {
        watchdog_log("ERROR", "Failed to create timer thread");
        return -1;
    }
#else
    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0) {
        watchdog_log("ERROR", "Failed to create timer thread");
        return -1;
    }
#endif
    
    return 0;
}

// OS-specific cleanup
void watchdog_os_cleanup(void) {
    if (timer_thread_running) {
        timer_thread_running = false;
#ifdef _WIN32
        WaitForSingleObject(timer_thread, INFINITE);
        CloseHandle(timer_thread);
        DeleteCriticalSection(&watchdog_mutex);
#else
        pthread_join(timer_thread, NULL);
#endif
    }
}

// OS-specific mutex operations
void watchdog_mutex_lock(void) {
#ifdef _WIN32
    EnterCriticalSection(&watchdog_mutex);
#else
    pthread_mutex_lock(&watchdog_mutex);
#endif
}

void watchdog_mutex_unlock(void) {
#ifdef _WIN32
    LeaveCriticalSection(&watchdog_mutex);
#else
    pthread_mutex_unlock(&watchdog_mutex);
#endif
}
