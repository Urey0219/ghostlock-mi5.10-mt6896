#ifndef GHOSTLOCK_FUTEX_PI_H
#define GHOSTLOCK_FUTEX_PI_H

#include <stdint.h>
#include <sys/types.h>

#define GL_FUTEX_LOCK_PI 6
#define GL_FUTEX_UNLOCK_PI 7
#define GL_FUTEX_WAIT_REQUEUE_PI 11
#define GL_FUTEX_CMP_REQUEUE_PI 12

#define GL_SYS_FUTEX 98
#define GL_SYS_SCHED_SETATTR 274
#define GL_SYS_PSELECT6 72
#define GL_SYS_SELECT 23

struct gl_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct gl_sched_attr {
    uint32_t size;
    uint32_t sched_policy;
    uint64_t sched_flags;
    int32_t sched_nice;
    uint32_t sched_priority;
    uint64_t sched_runtime;
    uint64_t sched_deadline;
    uint64_t sched_period;
};

#define GL_SCHED_BATCH 3

#define GL_SCHED_ATTR_SIZE 0x30

static inline long gl_futex_call(uint32_t *uaddr, int op, uint32_t val,
                                 const struct gl_timespec *timeout,
                                 uint32_t *uaddr2, uint32_t val3)
{
    register long x0 __asm__("x0") = (long)uaddr;
    register long x1 __asm__("x1") = (long)op;
    register long x2 __asm__("x2") = (long)val;
    register long x3 __asm__("x3") = (long)timeout;
    register long x4 __asm__("x4") = (long)uaddr2;
    register long x5 __asm__("x5") = (long)val3;
    register long x8 __asm__("x8") = GL_SYS_FUTEX;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x8)
                     : "memory");
    return x0;
}

static inline long gl_sched_setattr(pid_t tid, struct gl_sched_attr *attr,
                                    unsigned int flags)
{
    register long x0 __asm__("x0") = (long)tid;
    register long x1 __asm__("x1") = (long)attr;
    register long x2 __asm__("x2") = (long)flags;
    register long x8 __asm__("x8") = GL_SYS_SCHED_SETATTR;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x1), "r"(x2), "r"(x8)
                     : "memory");
    return x0;
}

#endif
