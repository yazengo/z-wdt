/*
 * Test program for embedded watchdog framework
 * Tests all major functionality in Linux environment
 */

#include "z_wdt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>

#ifdef _WIN32
    #include <windows.h>
    #include <process.h>
    #define usleep(x) Sleep((x)/1000)
    #define sleep(x) Sleep((x)*1000)
#else
    #include <unistd.h>
    #include <pthread.h>
#endif

// Test state
static bool test_running = true;
static int test_failures = 0;

// Test task data
typedef struct {
    int task_id;
    int channel_id;
    int feed_count;
    bool should_timeout;
    bool timeout_occurred;
} test_task_data_t;

// Test tasks
static test_task_data_t test_tasks[4];
#ifdef _WIN32
static HANDLE test_threads[4];
#else
static pthread_t test_threads[4];
#endif

// Signal handler for graceful shutdown
void signal_handler(int sig) {
    (void)sig;
    test_running = false;
    printf("\nReceived signal, stopping tests...\n");
}

// Watchdog timeout callback
void watchdog_timeout_callback(int channel_id, void *user_data) {
    test_task_data_t *task_data = (test_task_data_t *)user_data;
    
    printf("Watchdog timeout for channel %d (task %d)\n", channel_id, task_data->task_id);
    task_data->timeout_occurred = true;
}

// Test task 1: Normal operation (should not timeout)
void* test_task_1(void *arg) {
    test_task_data_t *data = (test_task_data_t *)arg;
    
    printf("Test task 1 started (channel %d)\n", data->channel_id);
    
    while (test_running && !data->timeout_occurred) {
        // Feed watchdog every 500ms
        if (z_wdt_feed(data->channel_id) == 0) {
            data->feed_count++;
            printf("Task 1 fed watchdog (count: %d)\n", data->feed_count);
        }
        
        usleep(500000); // 500ms
    }
    
    printf("Test task 1 finished\n");
    return NULL;
}

// Test task 2: Normal operation (should not timeout)
void* test_task_2(void *arg) {
    test_task_data_t *data = (test_task_data_t *)arg;
    
    printf("Test task 2 started (channel %d)\n", data->channel_id);
    
    while (test_running && !data->timeout_occurred) {
        // Feed watchdog every 800ms
        if (z_wdt_feed(data->channel_id) == 0) {
            data->feed_count++;
            printf("Task 2 fed watchdog (count: %d)\n", data->feed_count);
        }
        
        usleep(800000); // 800ms
    }
    
    printf("Test task 2 finished\n");
    return NULL;
}

// Test task 3: Will timeout (no feeding)
void* test_task_3(void *arg) {
    test_task_data_t *data = (test_task_data_t *)arg;
    
    printf("Test task 3 started (channel %d) - will timeout\n", data->channel_id);
    
    // Don't feed the watchdog, let it timeout
    while (test_running && !data->timeout_occurred) {
        usleep(100000); // 100ms
    }
    
    printf("Test task 3 finished (timeout occurred: %s)\n", 
           data->timeout_occurred ? "YES" : "NO");
    return NULL;
}

// Test task 4: Intermittent feeding (may timeout)
void* test_task_4(void *arg) {
    test_task_data_t *data = (test_task_data_t *)arg;
    
    printf("Test task 4 started (channel %d) - intermittent feeding\n", data->channel_id);
    
    int feed_interval = 0;
    while (test_running && !data->timeout_occurred) {
        feed_interval++;

        // Feed randomly
        int delay = (rand() % 3000) + 100;
        if (z_wdt_feed(data->channel_id) == 0) {
            data->feed_count++;
            printf("Task 4 fed watchdog (count: %d, delay: %d ms)\n",
                   data->feed_count, delay);
        }
        usleep(delay * 1000); // sleep for delay ms
    }

    printf("Test task 4 finished (timeout occurred: %s)\n", 
           data->timeout_occurred ? "YES" : "NO");
    return NULL;
}

// Test basic functionality
void test_basic_functionality(void) {
    printf("\n=== Testing Basic Functionality ===\n");
    
    // Test initialization
    assert(z_wdt_init() == 0);
    printf("✓ Watchdog initialization successful\n");
    
    // Test adding channels
    int channel1 = z_wdt_add(1000, watchdog_timeout_callback, &test_tasks[0]);
    assert(channel1 >= 0);
    printf("✓ Added channel %d with 1000ms timeout\n", channel1);
    
    int channel2 = z_wdt_add(2000, watchdog_timeout_callback, &test_tasks[1]);
    assert(channel2 >= 0);
    printf("✓ Added channel %d with 2000ms timeout\n", channel2);
    
    // Test feeding
    assert(z_wdt_feed(channel1) == 0);
    assert(z_wdt_feed(channel2) == 0);
    printf("✓ Feeding channels successful\n");
    
    // Test deletion
    assert(z_wdt_delete(channel2) == 0);
    printf("✓ Channel deletion successful\n");
    
    // Test invalid operations
    assert(z_wdt_feed(channel2) == -1); // Should fail after deletion
    assert(z_wdt_delete(channel1) == 0);
    printf("✓ Error handling working correctly\n");
}

// Test timeout functionality
void test_timeout_functionality(void) {
    printf("\n=== Testing Timeout Functionality ===\n");
    
    // Add a channel with short timeout
    int channel = z_wdt_add(1500, watchdog_timeout_callback, &test_tasks[0]);
    assert(channel >= 0);
    printf("✓ Added channel %d with 1500ms timeout\n", channel);
    
    // Feed once then wait for timeout
    assert(z_wdt_feed(channel) == 0);
    printf("✓ Fed channel, waiting for timeout...\n");
    
    // Wait for timeout (should occur within 2 seconds)
    sleep(3);
    
    // Check if timeout occurred
    if (test_tasks[0].timeout_occurred) {
        printf("✓ Timeout occurred as expected\n");
    } else {
        printf("✗ Timeout did not occur (test failed)\n");
        test_failures++;
    }
    
    // Clean up
    z_wdt_delete(channel);
}

// Test multiple channels
void test_multiple_channels(void) {
    printf("\n=== Testing Multiple Channels ===\n");
    
    // Initialize test tasks
    for (int i = 0; i < 4; i++) {
        test_tasks[i].task_id = i;
        test_tasks[i].feed_count = 0;
        test_tasks[i].timeout_occurred = false;
        test_tasks[i].should_timeout = (i >= 2); // Tasks 2 and 3 should timeout
    }
    
    // Add channels
    test_tasks[0].channel_id = z_wdt_add(2000, watchdog_timeout_callback, &test_tasks[0]);
    test_tasks[1].channel_id = z_wdt_add(3000, watchdog_timeout_callback, &test_tasks[1]);
    test_tasks[2].channel_id = z_wdt_add(1000, watchdog_timeout_callback, &test_tasks[2]);
    test_tasks[3].channel_id = z_wdt_add(1000, watchdog_timeout_callback, &test_tasks[3]);
    
    assert(test_tasks[0].channel_id >= 0);
    assert(test_tasks[1].channel_id >= 0);
    assert(test_tasks[2].channel_id >= 0);
    assert(test_tasks[3].channel_id >= 0);
    printf("✓ Added 4 channels successfully\n");
    
    // Create test threads
#ifdef _WIN32
    test_threads[0] = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))test_task_1, &test_tasks[0], 0, NULL);
    test_threads[1] = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))test_task_2, &test_tasks[1], 0, NULL);
    test_threads[2] = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))test_task_3, &test_tasks[2], 0, NULL);
    test_threads[3] = (HANDLE)_beginthreadex(NULL, 0, (unsigned (__stdcall *)(void *))test_task_4, &test_tasks[3], 0, NULL);
#else
    pthread_create(&test_threads[0], NULL, test_task_1, &test_tasks[0]);
    pthread_create(&test_threads[1], NULL, test_task_2, &test_tasks[1]);
    pthread_create(&test_threads[2], NULL, test_task_3, &test_tasks[2]);
    pthread_create(&test_threads[3], NULL, test_task_4, &test_tasks[3]);
#endif
    
    printf("✓ Created 4 test threads\n");
    
    // Let them run for 10 seconds
    printf("Running test for 10 seconds...\n");
    sleep(10);
    
    // Check results
    printf("\nTest results:\n");
    for (int i = 0; i < 4; i++) {
        printf("Task %d: feeds=%d, timeout=%s\n", 
               i, test_tasks[i].feed_count, 
               test_tasks[i].timeout_occurred ? "YES" : "NO");
        
        if (i < 2 && test_tasks[i].timeout_occurred) {
            printf("✗ Task %d should not have timed out\n", i);
            test_failures++;
        } else if (i >= 2 && !test_tasks[i].timeout_occurred) {
            printf("✗ Task %d should have timed out\n", i);
            test_failures++;
        }
    }
    
    // Clean up threads
    test_running = false;
    for (int i = 0; i < 4; i++) {
#ifdef _WIN32
        WaitForSingleObject(test_threads[i], INFINITE);
        CloseHandle(test_threads[i]);
#else
        pthread_join(test_threads[i], NULL);
#endif
        z_wdt_delete(test_tasks[i].channel_id);
    }
}

// Test suspend/resume functionality
void test_suspend_resume(void) {
    printf("\n=== Testing Suspend/Resume Functionality ===\n");
    
    // Add a channel
    int channel = z_wdt_add(2000, watchdog_timeout_callback, &test_tasks[0]);
    assert(channel >= 0);
    
    // Feed the channel
    assert(z_wdt_feed(channel) == 0);
    printf("✓ Fed channel before suspend\n");
    
    // Suspend watchdog
    z_wdt_suspend();
    printf("✓ Watchdog suspended\n");
    
    // Wait longer than timeout period
    sleep(3);
    printf("✓ Waited 3 seconds (longer than 2s timeout)\n");
    
    // Resume watchdog
    z_wdt_resume();
    printf("✓ Watchdog resumed\n");
    
    // Feed again
    assert(z_wdt_feed(channel) == 0);
    printf("✓ Fed channel after resume\n");
    
    // Wait for timeout
    sleep(3);
    
    if (test_tasks[0].timeout_occurred) {
        printf("✓ Timeout occurred after resume (expected)\n");
    } else {
        printf("✗ Timeout did not occur after resume\n");
        test_failures++;
    }
    
    // Clean up
    z_wdt_delete(channel);
}

// Test error conditions
void test_error_conditions(void) {
    printf("\n=== Testing Error Conditions ===\n");
    
    // Test operations before initialization
    z_wdt_cleanup(); // Clean up previous state
    assert(z_wdt_add(1000, NULL, NULL) == -1);
    assert(z_wdt_feed(0) == -1);
    assert(z_wdt_delete(0) == -1);
    printf("✓ Error handling before initialization works\n");
    
    // Re-initialize
    assert(z_wdt_init() == 0);
    
    // Test invalid channel IDs
    assert(z_wdt_feed(-1) == -1);
    assert(z_wdt_feed(WATCHDOG_MAX_CHANNELS) == -1);
    assert(z_wdt_delete(-1) == -1);
    assert(z_wdt_delete(WATCHDOG_MAX_CHANNELS) == -1);
    printf("✓ Invalid channel ID handling works\n");
    
    // Test invalid reload period
    assert(z_wdt_add(0, NULL, NULL) == -1);
    printf("✓ Invalid reload period handling works\n");
    
    // Test feeding non-existent channel
    assert(z_wdt_feed(0) == -1);
    printf("✓ Feeding non-existent channel handling works\n");
}

// Test maximum channels
void test_maximum_channels(void) {
    printf("\n=== Testing Maximum Channels ===\n");
    
    int channels[WATCHDOG_MAX_CHANNELS];
    
    // Add maximum number of channels
    for (int i = 0; i < WATCHDOG_MAX_CHANNELS; i++) {
        channels[i] = z_wdt_add(1000 + i, watchdog_timeout_callback, NULL);
        assert(channels[i] >= 0);
    }
    printf("✓ Added %d channels successfully\n", WATCHDOG_MAX_CHANNELS);
    
    // Try to add one more (should fail)
    int extra_channel = z_wdt_add(1000, watchdog_timeout_callback, NULL);
    assert(extra_channel == -1);
    printf("✓ Correctly rejected extra channel\n");
    
    // Clean up all channels
    for (int i = 0; i < WATCHDOG_MAX_CHANNELS; i++) {
        assert(z_wdt_delete(channels[i]) == 0);
    }
    printf("✓ Cleaned up all channels\n");
}

int main(void) {
    printf("Embedded Watchdog Framework Test Suite\n");
    printf("=====================================\n");
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Run tests
    test_basic_functionality();
    test_timeout_functionality();
    test_multiple_channels();
    test_suspend_resume();
    test_error_conditions();
    test_maximum_channels();
    
    // Clean up
    z_wdt_cleanup();
    
    // Print results
    printf("\n=== Test Results ===\n");
    if (test_failures == 0) {
        printf("✓ All tests passed!\n");
    } else {
        printf("✗ %d test(s) failed\n", test_failures);
    }
    
    return test_failures;
}
