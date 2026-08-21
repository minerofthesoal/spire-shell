#include "jobs.h"
#include <sys/wait.h>
#include <stdio.h>

static Job *g_jobs = NULL;
static int g_next_id = 1;

void jobs_init(void) { g_jobs = NULL; g_next_id = 1; }

Job *job_add(pid_t pgid, const char *cmdline) {
    Job *j = xmalloc(sizeof(Job));
    j->id = g_next_id++;
    j->pgid = pgid;
    j->cmdline = xstrdup(cmdline);
    j->state = JOB_RUNNING;
    j->last_status = 0;
    j->notified = false;
    j->next = g_jobs;
    g_jobs = j;
    return j;
}

void job_remove(Job *j) {
    Job **pp = &g_jobs;
    while (*pp) {
        if (*pp == j) {
            *pp = j->next;
            free(j->cmdline);
            free(j);
            return;
        }
        pp = &(*pp)->next;
    }
}

Job *job_find(int id) {
    for (Job *j = g_jobs; j; j = j->next) if (j->id == id) return j;
    return NULL;
}

Job *job_find_by_pgid(pid_t pgid) {
    for (Job *j = g_jobs; j; j = j->next) if (j->pgid == pgid) return j;
    return NULL;
}

int jobs_count(void) {
    int n = 0;
    for (Job *j = g_jobs; j; j = j->next) if (j->state != JOB_DONE) n++;
    return n;
}

void jobs_reap(bool print_notices) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        for (Job *j = g_jobs; j; j = j->next) {
            if (j->pgid == pid) {
                if (WIFSTOPPED(status)) { j->state = JOB_STOPPED; }
                else {
                    j->state = JOB_DONE;
                    j->last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
                }
            }
        }
    }
    if (print_notices) {
        Job *j = g_jobs, *prev = NULL;
        while (j) {
            Job *next = j->next;
            if (j->state == JOB_DONE) {
                if (!j->notified) {
                    fprintf(stderr, "[%d]  Done                    %s\n", j->id, j->cmdline);
                }
                if (prev) prev->next = next; else g_jobs = next;
                free(j->cmdline);
                free(j);
                j = next;
                continue;
            }
            prev = j;
            j = next;
        }
    }
}

void jobs_print(void) {
    for (Job *j = g_jobs; j; j = j->next) {
        const char *state = j->state == JOB_RUNNING ? "Running" : j->state == JOB_STOPPED ? "Stopped" : "Done";
        printf("[%d]  %-8s  %s\n", j->id, state, j->cmdline);
    }
}
