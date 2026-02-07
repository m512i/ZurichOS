#ifndef _SHELL_FEATURES_H
#define _SHELL_FEATURES_H

#include <stdint.h>

void env_init(void);
int env_set(const char *key, const char *value);
const char *env_get(const char *key);
int env_unset(const char *key);
int env_expand(const char *input, char *output, int max_len);
void env_list(void);
int env_count(void);

#define REDIR_NONE      0
#define REDIR_OUT       1   
#define REDIR_APPEND    2   
#define REDIR_IN        3   

typedef struct {
    int type;
    char filename[128];
} redirect_t;

#define MAX_JOBS        8
#define JOB_RUNNING     1
#define JOB_STOPPED     2
#define JOB_DONE        3

typedef struct {
    int id;
    int state;
    char command[128];
    int in_use;
} job_t;

void jobs_init(void);
int job_add(const char *command);
void job_remove(int job_id);
void job_set_state(int job_id, int state);
void jobs_list(void);
void jobs_check(void);
int jobs_count(void);

int shell_run_script(const char *path);

void cmd_export(int argc, char **argv);
void cmd_unset(int argc, char **argv);
void cmd_env(int argc, char **argv);
void cmd_set(int argc, char **argv);
void cmd_source(int argc, char **argv);
void cmd_jobs(int argc, char **argv);
void cmd_fg(int argc, char **argv);
void cmd_history(int argc, char **argv);
void cmd_alias(int argc, char **argv);

#endif
