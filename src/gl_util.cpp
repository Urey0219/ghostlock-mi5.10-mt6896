#include "common.h"

std::atomic<int> g_t1_ready{0};
std::atomic<int> g_owner_started{0};
std::atomic<int> g_owner_chain_done{0};
std::atomic<int> g_route_done{0};
std::atomic<int> g_t1_tid{0};
std::atomic<int> g_consumer_go{0};
std::atomic<int> g_arm_go{0};
std::atomic<int> g_consume_go{0};
std::atomic<int> g_route_result{0};
std::atomic<int> g_route_errno{0};
std::atomic<int> g_route_stage{0};
std::atomic<int> g_creds_armed{0};
std::atomic<int> g_creds_parked{0};

std::atomic<int> g_lock_chain{0};
std::atomic<int> g_lock_target{0};
std::atomic<int> g_lock_fake_a{0};
std::atomic<int> g_lock_fake_b{0};
std::atomic<int> g_lock_stage{0};
std::atomic<int> g_lock_owner_done{0};
std::atomic<int> g_chain_released{0};
std::atomic<int> g_owner_blocked{0};
std::atomic<int> g_t1_entered{0};
std::atomic<int> g_lock_route_started{0};
std::atomic<int> g_rearm_go{0};
std::atomic<int> g_rearm_done{0};
std::atomic<int> g_cycle_fresh{0};

std::atomic<int> g_drain_go{0};
std::atomic<int> g_drain_unlock_done{0};
std::atomic<int> g_drain_done{0};
std::atomic<int> g_drain_ret{-999};

uint64_t g_scratch_lo;
uint64_t g_fake_lock;
uint64_t g_init_area;

std::atomic<int> g_w_go{0};
std::atomic<int> g_w_done{0};
uint64_t g_w_target;
uint64_t g_w_value;
uint64_t g_w_lock;

uint64_t gl_monotonic_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * GL_MS_PER_SEC +
           (uint64_t)ts.tv_nsec / GL_NS_PER_MS;
}

int gl_get_tid(void)
{
    return (int)syscall(SYS_gettid);
}

void gl_yield_spin_flag(std::atomic<int> *flag, int value)
{
    while (flag->load(std::memory_order_acquire) != value)
        sched_yield();
}

int gl_wait_flag(std::atomic<int> *flag, int value, uint64_t timeout_ms)
{
    uint64_t deadline = gl_monotonic_ms() + timeout_ms;
    while (flag->load(std::memory_order_acquire) != value) {
        if (gl_monotonic_ms() > deadline)
            return -1;
        sched_yield();
    }
    return 0;
}

int gl_drain_pi_cycle(void)
{
    int ret;

    if (g_t1_tid.load(std::memory_order_acquire) == 0)
        return 0;

    g_drain_go.store(1, std::memory_order_release);

    if (gl_wait_flag(&g_drain_done, 1, GL_DRAIN_WAIT_MS) != 0)
        return -1;

    ret = g_drain_ret.load(std::memory_order_acquire);

    return ret == 0 ? 0 : -1;
}

void gl_drain_or_park(void)
{
    if (gl_drain_pi_cycle() == 0)
        return;

    fputs("failed\n", stderr);

    for (;;)
        pause();
}
