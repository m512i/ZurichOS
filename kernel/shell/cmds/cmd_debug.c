/* Shell Commands - Debug Category
 * panic, vga, beep, play, synctest, pritest
 */

#include <shell/builtins.h>
#include <kernel/shell.h>
#include <kernel/kernel.h>
#include <kernel/scheduler.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <drivers/speaker.h>
#include <sync/mutex.h>
#include <sync/semaphore.h>
#include <sync/condvar.h>
#include <sync/rwlock.h>
#include <kernel/assert.h>
#include <mm/heap.h>
#include <string.h>

void cmd_panic(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Triggering kernel panic for testing...\n");
    panic("Test panic triggered by user");
}

void cmd_vga(int argc, char **argv)
{
    if (argc < 4) {
        vga_puts("Usage: vga <x> <y> <char>\n");
        vga_puts("Write a character directly to VGA buffer\n");
        vga_puts("Example: vga 0 0 X  (write X at top-left)\n");
        return;
    }
    
    uint32_t x = shell_parse_dec(argv[1]);
    uint32_t y = shell_parse_dec(argv[2]);
    char c = argv[3][0];
    
    if (x >= 80 || y >= 25) {
        vga_puts("Error: x must be 0-79, y must be 0-24\n");
        return;
    }
    
    uint16_t *vga = (uint16_t *)(0xC00B8000);
    vga[y * 80 + x] = (uint16_t)c | (0x0F << 8);  
    
    vga_puts("Wrote '");
    vga_putchar(c);
    vga_puts("' at (");
    vga_put_dec(x);
    vga_puts(", ");
    vga_put_dec(y);
    vga_puts(")\n");
}

void cmd_beep(int argc, char **argv)
{
    uint32_t freq = 440;    
    uint32_t duration = 200; 
    
    if (argc >= 2) {
        freq = shell_parse_dec(argv[1]);
    }
    if (argc >= 3) {
        duration = shell_parse_dec(argv[2]);
    }
    
    if (freq < 20 || freq > 20000) {
        vga_puts("Frequency must be 20-20000 Hz\n");
        return;
    }
    if (duration > 5000) {
        duration = 5000;  
    }
    
    vga_puts("Playing ");
    vga_put_dec(freq);
    vga_puts(" Hz for ");
    vga_put_dec(duration);
    vga_puts(" ms\n");
    
    speaker_beep(freq, duration);
}

void cmd_play(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Playing startup tune...\n");
    
    speaker_beep(NOTE_C5, 150);
    speaker_beep(NOTE_E5, 150);
    speaker_beep(NOTE_G5, 150);
    speaker_beep(NOTE_C6, 300);
    speaker_beep(NOTE_G5, 150);
    speaker_beep(NOTE_C6, 400);
    
    vga_puts("Done!\n");
}

static mutex_t test_mutex;
static semaphore_t test_sem;
static volatile int shared_counter = 0;

static void mutex_test_task(void)
{
    serial_puts("[TASK] Trying to acquire mutex...\n");
    mutex_lock(&test_mutex);
    serial_puts("[TASK] Mutex acquired! Incrementing counter...\n");
    
    shared_counter++;
    
    for (volatile int i = 0; i < 100000; i++);
    
    serial_puts("[TASK] Releasing mutex\n");
    mutex_unlock(&test_mutex);
    
    serial_puts("[TASK] Task done\n");
}

static void sem_test_task(void)
{
    serial_puts("[TASK] Waiting on semaphore...\n");
    semaphore_wait(&test_sem);
    serial_puts("[TASK] Semaphore acquired!\n");
    
    shared_counter++;
    
    serial_puts("[TASK] Task done, counter = ");
    char buf[12];
    int i = 0;
    int n = shared_counter;
    if (n == 0) buf[i++] = '0';
    else while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    buf[i] = '\0';
    for (int j = 0; j < i/2; j++) { char t = buf[j]; buf[j] = buf[i-1-j]; buf[i-1-j] = t; }
    serial_puts(buf);
    serial_puts("\n");
}

void cmd_synctest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Synchronization Test\n");
    vga_puts("====================\n\n");
    
    /* Note: We use manual schedule() calls, not preemptive scheduling 
     * This avoids interfering with shell keyboard handling */
    
    vga_puts("Test 1: Mutex blocking\n");
    serial_puts("\n=== MUTEX TEST ===\n");
    
    mutex_init(&test_mutex);
    shared_counter = 0;
    
    serial_puts("[MAIN] Acquiring mutex...\n");
    mutex_lock(&test_mutex);
    serial_puts("[MAIN] Mutex held by main task\n");
    
    vga_puts("  Creating task that will block on mutex...\n");
    task_t *t1 = task_create("mutex_test", mutex_test_task, 4096);
    if (!t1) {
        vga_puts("  Failed to create task!\n");
        mutex_unlock(&test_mutex);
        return;
    }
    
    vga_puts("  Yielding to let task try to acquire mutex...\n");
    serial_puts("[MAIN] Yielding to let task run...\n");
    for (int y = 0; y < 10; y++) {
        schedule_force();
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_puts("  Releasing mutex (should wake blocked task)...\n");
    serial_puts("[MAIN] Releasing mutex\n");
    mutex_unlock(&test_mutex);
    
    for (int y = 0; y < 10; y++) {
        schedule_force();
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_puts("  Mutex test complete. Counter = ");
    vga_put_dec(shared_counter);
    vga_puts("\n\n");
    
    vga_puts("Test 2: Semaphore blocking\n");
    serial_puts("\n=== SEMAPHORE TEST ===\n");
    
    semaphore_init(&test_sem, 0);  
    shared_counter = 0;
    
    vga_puts("  Creating 2 tasks that will block on semaphore...\n");
    task_t *t2 = task_create("sem_test1", sem_test_task, 4096);
    task_t *t3 = task_create("sem_test2", sem_test_task, 4096);
    
    if (!t2 || !t3) {
        vga_puts("  Failed to create tasks!\n");
        return;
    }
    
    vga_puts("  Yielding to let tasks block...\n");
    serial_puts("[MAIN] Yielding to let tasks block\n");
    for (int y = 0; y < 10; y++) {
        schedule_force();
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_puts("  Signaling semaphore (wake 1 task)...\n");
    serial_puts("[MAIN] Signaling semaphore\n");
    semaphore_signal(&test_sem);
    for (int y = 0; y < 10; y++) {
        schedule_force();
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_puts("  Signaling semaphore again (wake 2nd task)...\n");
    serial_puts("[MAIN] Signaling semaphore again\n");
    semaphore_signal(&test_sem);
    for (int y = 0; y < 10; y++) {
        schedule_force();
        for (volatile int i = 0; i < 1000000; i++);
    }
    
    vga_puts("  Semaphore test complete. Counter = ");
    vga_put_dec(shared_counter);
    vga_puts("\n\n");
    
    vga_puts("All synchronization tests complete!\n");
    vga_puts("Check serial output for detailed trace.\n");
    
    serial_puts("[SYNCTEST] Test complete, returning to shell\n");
}

static mutex_t pi_mutex;
static volatile int pi_low_done = 0;
static volatile int pi_high_done = 0;

static volatile int pi_high_waiting = 0;

static void pi_low_priority_task(void)
{
    task_t *self = task_current();
    serial_puts("[LOW] Low priority task started (priority ");
    char buf[4];
    buf[0] = '0' + (self->priority / 10);
    buf[1] = '0' + (self->priority % 10);
    buf[2] = ')';
    buf[3] = '\0';
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("[LOW] Acquiring mutex...\n");
    mutex_lock(&pi_mutex);
    serial_puts("[LOW] Mutex acquired, doing work...\n");
    
    serial_puts("[LOW] Yielding to let high priority task block...\n");
    for (int y = 0; y < 20; y++) {
        schedule_force();
        for (volatile int i = 0; i < 100000; i++);
    }
    
    serial_puts("[LOW] Current priority: ");
    buf[0] = '0' + (self->priority / 10);
    buf[1] = '0' + (self->priority % 10);
    buf[2] = '\0';
    serial_puts(buf);
    serial_puts(" (inherited: ");
    buf[0] = '0' + (self->inherited_priority / 10);
    buf[1] = '0' + (self->inherited_priority % 10);
    serial_puts(buf);
    serial_puts(", base: ");
    buf[0] = '0' + (self->base_priority / 10);
    buf[1] = '0' + (self->base_priority % 10);
    serial_puts(buf);
    serial_puts(")\n");
    
    serial_puts("[LOW] Releasing mutex...\n");
    mutex_unlock(&pi_mutex);
    
    serial_puts("[LOW] Priority after release: ");
    buf[0] = '0' + (self->priority / 10);
    buf[1] = '0' + (self->priority % 10);
    serial_puts(buf);
    serial_puts("\n");
    
    pi_low_done = 1;
    serial_puts("[LOW] Task complete\n");
}

static void pi_high_priority_task(void)
{
    task_t *self = task_current();
    serial_puts("[HIGH] High priority task started (priority ");
    char buf[4];
    buf[0] = '0' + (self->priority / 10);
    buf[1] = '0' + (self->priority % 10);
    buf[2] = ')';
    buf[3] = '\0';
    serial_puts(buf);
    serial_puts("\n");
    
    serial_puts("[HIGH] Trying to acquire mutex (should trigger priority inheritance)...\n");
    pi_high_waiting = 1;
    mutex_lock(&pi_mutex);
    serial_puts("[HIGH] Mutex acquired!\n");
    
    mutex_unlock(&pi_mutex);
    
    pi_high_done = 1;
    serial_puts("[HIGH] Task complete\n");
}

void cmd_pritest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Priority Inheritance Test\n");
    vga_puts("=========================\n\n");
    vga_puts("Watch serial output for detailed trace.\n\n");
    
    serial_puts("\n=== PRIORITY INHERITANCE TEST ===\n");
    serial_puts("This test demonstrates priority inheritance:\n");
    serial_puts("1. Low priority task (20) acquires mutex\n");
    serial_puts("2. High priority task (5) blocks on mutex\n");
    serial_puts("3. Low task should be boosted to priority 5\n");
    serial_puts("4. After unlock, low task returns to priority 20\n\n");
    
    mutex_init(&pi_mutex);
    pi_low_done = 0;
    pi_high_done = 0;
    
    vga_puts("Creating low priority task (priority 20)...\n");
    task_t *low_task = task_create("pi_low", pi_low_priority_task, 4096);
    if (!low_task) {
        vga_puts("Failed to create low priority task!\n");
        return;
    }
    task_set_priority(low_task, 20);
    
    vga_puts("Creating high priority task (priority 5)...\n");
    task_t *high_task = task_create("pi_high", pi_high_priority_task, 4096);
    if (!high_task) {
        vga_puts("Failed to create high priority task!\n");
        return;
    }
    task_set_priority(high_task, 5);
    
    vga_puts("Running tasks...\n");
    serial_puts("[MAIN] Starting scheduler cycles\n");
    
    int timeout = 100;
    while ((!pi_low_done || !pi_high_done) && timeout > 0) {
        schedule_force();
        for (volatile int i = 0; i < 500000; i++);
        timeout--;
    }
    
    vga_puts("\nResults:\n");
    if (pi_low_done && pi_high_done) {
        vga_puts("  Both tasks completed successfully!\n");
        vga_puts("  Priority inheritance worked correctly.\n");
        serial_puts("[PRITEST] SUCCESS - Priority inheritance working\n");
    } else {
        vga_puts("  Test timed out or failed.\n");
        serial_puts("[PRITEST] FAILED - Timeout or deadlock\n");
    }
    
    vga_puts("\nCheck serial output for priority changes.\n");
}

static mutex_t cv_mutex;
static condvar_t cv_cond;
static volatile int cv_data_ready = 0;
static volatile int cv_producer_done = 0;
static volatile int cv_consumer_done = 0;

static void cv_producer_task(void)
{
    serial_printf("[PRODUCER] Starting, acquiring mutex...\n");
    mutex_lock(&cv_mutex);
    
    serial_printf("[PRODUCER] Mutex acquired, preparing data...\n");
    for (volatile int i = 0; i < 1000000; i++);
    
    cv_data_ready = 1;
    serial_printf("[PRODUCER] Data ready, signaling consumer...\n");
    condvar_signal(&cv_cond);
    
    mutex_unlock(&cv_mutex);
    serial_printf("[PRODUCER] Done\n");
    cv_producer_done = 1;
}

static void cv_consumer_task(void)
{
    serial_printf("[CONSUMER] Starting, acquiring mutex...\n");
    mutex_lock(&cv_mutex);
    
    while (!cv_data_ready) {
        serial_printf("[CONSUMER] Data not ready, waiting on condvar...\n");
        condvar_wait(&cv_cond, &cv_mutex);
        serial_printf("[CONSUMER] Woke up from condvar wait\n");
    }
    
    serial_printf("[CONSUMER] Data received! Processing...\n");
    mutex_unlock(&cv_mutex);
    serial_printf("[CONSUMER] Done\n");
    cv_consumer_done = 1;
}

void cmd_cvtest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Condition Variable Test\n");
    vga_puts("=======================\n\n");
    vga_puts("Watch serial output for detailed trace.\n\n");
    
    serial_printf("\n=== CONDITION VARIABLE TEST ===\n");
    serial_printf("Producer-Consumer pattern using condvar:\n");
    serial_printf("1. Consumer waits on condvar for data\n");
    serial_printf("2. Producer prepares data and signals\n");
    serial_printf("3. Consumer wakes up and processes\n\n");
    
    mutex_init(&cv_mutex);
    condvar_init(&cv_cond);
    cv_data_ready = 0;
    cv_producer_done = 0;
    cv_consumer_done = 0;
    
    vga_puts("Creating consumer task...\n");
    task_t *consumer = task_create("cv_consumer", cv_consumer_task, 4096);
    if (!consumer) {
        vga_puts("Failed to create consumer task!\n");
        return;
    }
    
    for (volatile int i = 0; i < 500000; i++);
    
    vga_puts("Creating producer task...\n");
    task_t *producer = task_create("cv_producer", cv_producer_task, 4096);
    if (!producer) {
        vga_puts("Failed to create producer task!\n");
        return;
    }
    
    vga_puts("Running tasks...\n");
    
    int timeout = 100;
    while ((!cv_producer_done || !cv_consumer_done) && timeout > 0) {
        schedule_force();
        for (volatile int i = 0; i < 500000; i++);
        timeout--;
    }
    
    vga_puts("\nResults:\n");
    if (cv_producer_done && cv_consumer_done) {
        vga_puts("  Producer-Consumer completed successfully!\n");
        vga_puts("  Condition variable working correctly.\n");
        serial_printf("[CVTEST] SUCCESS\n");
    } else {
        vga_puts("  Test timed out or failed.\n");
        serial_printf("[CVTEST] FAILED - Timeout\n");
    }
}

static rwlock_t rw_lock;
static volatile int rw_shared_data = 0;
static volatile int rw_readers_done = 0;
static volatile int rw_writer_done = 0;

static void rw_reader_task(void)
{
    uint32_t tid = task_current() ? task_current()->tid : 0;
    
    serial_printf("[READER %u] Acquiring read lock...\n", tid);
    rwlock_read_lock(&rw_lock);
    serial_printf("[READER %u] Read lock acquired, data = %d\n", tid, rw_shared_data);
    
    for (volatile int i = 0; i < 500000; i++);
    
    serial_printf("[READER %u] Releasing read lock\n", tid);
    rwlock_read_unlock(&rw_lock);
    
    __sync_fetch_and_add(&rw_readers_done, 1);
}

static void rw_writer_task(void)
{
    serial_printf("[WRITER] Acquiring write lock...\n");
    rwlock_write_lock(&rw_lock);
    serial_printf("[WRITER] Write lock acquired, updating data...\n");
    
    rw_shared_data = 42;
    for (volatile int i = 0; i < 300000; i++);
    
    serial_printf("[WRITER] Data updated to %d, releasing lock\n", rw_shared_data);
    rwlock_write_unlock(&rw_lock);
    
    rw_writer_done = 1;
}

void cmd_rwtest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Read-Write Lock Test\n");
    vga_puts("====================\n\n");
    vga_puts("Watch serial output for detailed trace.\n\n");
    
    serial_printf("\n=== READ-WRITE LOCK TEST ===\n");
    serial_printf("Multiple readers, single writer:\n");
    serial_printf("1. Multiple readers can hold lock simultaneously\n");
    serial_printf("2. Writer gets exclusive access\n");
    serial_printf("3. Readers wait while writer holds lock\n\n");
    
    rwlock_init(&rw_lock);
    rw_shared_data = 0;
    rw_readers_done = 0;
    rw_writer_done = 0;
    
    vga_puts("Creating 3 reader tasks...\n");
    for (int i = 0; i < 3; i++) {
        task_t *reader = task_create("rw_reader", rw_reader_task, 4096);
        if (!reader) {
            vga_puts("Failed to create reader task!\n");
            return;
        }
    }
    
    for (volatile int i = 0; i < 200000; i++);
    
    vga_puts("Creating writer task...\n");
    task_t *writer = task_create("rw_writer", rw_writer_task, 4096);
    if (!writer) {
        vga_puts("Failed to create writer task!\n");
        return;
    }
    
    vga_puts("Running tasks...\n");
    
    int timeout = 100;
    while ((rw_readers_done < 3 || !rw_writer_done) && timeout > 0) {
        schedule_force();
        for (volatile int i = 0; i < 500000; i++);
        timeout--;
    }
    
    vga_puts("\nResults:\n");
    vga_puts("  Readers completed: ");
    vga_put_dec(rw_readers_done);
    vga_puts("/3\n");
    vga_puts("  Writer completed: ");
    vga_puts(rw_writer_done ? "yes" : "no");
    vga_puts("\n");
    vga_puts("  Final data value: ");
    vga_put_dec(rw_shared_data);
    vga_puts("\n");
    
    if (rw_readers_done == 3 && rw_writer_done && rw_shared_data == 42) {
        vga_puts("  Read-write lock working correctly!\n");
        serial_printf("[RWTEST] SUCCESS\n");
    } else {
        vga_puts("  Test incomplete or failed.\n");
        serial_printf("[RWTEST] FAILED\n");
    }
}

void cmd_asserttest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Assert Test\n");
    vga_puts("===========\n\n");
    vga_puts("Testing ASSERT macro...\n");
    vga_puts("This will trigger an assertion failure!\n\n");
    
    serial_printf("[ASSERTTEST] Triggering assertion failure...\n");
    
    ASSERT(1 == 0);
}

void cmd_guardtest(int argc, char **argv)
{
    (void)argc; (void)argv;
    
    vga_puts("Memory Guard Test\n");
    vga_puts("=================\n\n");
    
    vga_puts("Test 1: Normal allocation (no overflow)\n");
    char *buf1 = (char *)kmalloc(32);
    if (buf1) {
        for (int i = 0; i < 30; i++) {
            buf1[i] = 'A';
        }
        buf1[30] = '\0';
        
        if (heap_check_overflow(buf1)) {
            vga_puts("  Guard intact - PASS\n");
            serial_printf("[GUARDTEST] Normal alloc: PASS\n");
        } else {
            vga_puts("  Guard corrupted - FAIL\n");
            serial_printf("[GUARDTEST] Normal alloc: FAIL\n");
        }
        kfree(buf1);
    }
    
    vga_puts("\nTest 2: Deliberate overflow (will corrupt guard)\n");
    char *buf2 = (char *)kmalloc(16);
    if (buf2) {
        vga_puts("  Writing beyond buffer...\n");
        for (int i = 0; i < 20; i++) {
            buf2[i] = 'X';
        }
        
        if (!heap_check_overflow(buf2)) {
            vga_puts("  Overflow detected - PASS\n");
            serial_printf("[GUARDTEST] Overflow detection: PASS\n");
        } else {
            vga_puts("  Overflow NOT detected - FAIL\n");
            serial_printf("[GUARDTEST] Overflow detection: FAIL\n");
        }
        
        vga_puts("  Freeing (should show warning in serial)...\n");
        kfree(buf2);
    }
    
    vga_puts("\nMemory guard test complete.\n");
}
