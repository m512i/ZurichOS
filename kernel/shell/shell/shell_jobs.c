/* Shell Job Control
 * Background jobs, fg, jobs command
 */

#include <shell/shell_features.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <string.h>

static job_t jobs[MAX_JOBS];
static int next_job_id = 1;

void jobs_init(void)
{
    memset(jobs, 0, sizeof(jobs));
    next_job_id = 1;
}

int job_add(const char *command)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].in_use) {
            jobs[i].id = next_job_id++;
            jobs[i].state = JOB_RUNNING;
            strncpy(jobs[i].command, command, 127);
            jobs[i].command[127] = '\0';
            jobs[i].in_use = 1;
            return jobs[i].id;
        }
    }
    return -1;
}

void job_remove(int job_id)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && jobs[i].id == job_id) {
            jobs[i].in_use = 0;
            return;
        }
    }
}

void job_set_state(int job_id, int state)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && jobs[i].id == job_id) {
            jobs[i].state = state;
            return;
        }
    }
}

void jobs_list(void)
{
    int found = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use) {
            found = 1;
            vga_puts("[");
            char buf[8];
            int n = jobs[i].id;
            int idx = 0;
            if (n == 0) { buf[idx++] = '0'; }
            else { 
                char tmp[8]; int ti = 0;
                while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
                while (ti > 0) buf[idx++] = tmp[--ti];
            }
            buf[idx] = '\0';
            vga_puts(buf);
            vga_puts("]  ");
            
            switch (jobs[i].state) {
                case JOB_RUNNING: vga_puts("Running    "); break;
                case JOB_STOPPED: vga_puts("Stopped    "); break;
                case JOB_DONE:    vga_puts("Done       "); break;
            }
            
            vga_puts(jobs[i].command);
            vga_puts(" &\n");
        }
    }
    if (!found) {
        vga_puts("No active jobs.\n");
    }
}

void jobs_check(void)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use && jobs[i].state == JOB_DONE) {
            vga_puts("[");
            char buf[8];
            int n = jobs[i].id;
            int idx = 0;
            if (n == 0) { buf[idx++] = '0'; }
            else {
                char tmp[8]; int ti = 0;
                while (n > 0) { tmp[ti++] = '0' + (n % 10); n /= 10; }
                while (ti > 0) buf[idx++] = tmp[--ti];
            }
            buf[idx] = '\0';
            vga_puts(buf);
            vga_puts("]  Done       ");
            vga_puts(jobs[i].command);
            vga_puts("\n");
            jobs[i].in_use = 0;
        }
    }
}

int jobs_count(void)
{
    int count = 0;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].in_use) count++;
    }
    return count;
}
