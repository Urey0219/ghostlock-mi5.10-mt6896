#ifndef GHOSTLOCK_LAYOUT_H
#define GHOSTLOCK_LAYOUT_H

#include <stdint.h>

struct rb_node {
    uint64_t __rb_parent_color;
    struct rb_node *rb_right;
    struct rb_node *rb_left;
};

struct rb_root {
    struct rb_node *rb_node;
};

struct rb_root_cached {
    struct rb_root rb_root;
    struct rb_node *rb_leftmost;
};

struct rt_mutex_waiter {
    struct rb_node tree_entry;
    struct rb_node pi_tree_entry;
    void *task;
    void *lock;
    int32_t prio;
    uint32_t pad;
    uint64_t deadline;
};

struct rt_mutex {
    uint32_t wait_lock;
    uint32_t pad;
    struct rb_root_cached waiters;
    void *owner;
};

enum {
    GL_WAITER_TREE_PARENT = 0x00,
    GL_WAITER_TREE_RIGHT = 0x08,
    GL_WAITER_TREE_LEFT = 0x10,
    GL_WAITER_PI_PARENT = 0x18,
    GL_WAITER_PI_RIGHT = 0x20,
    GL_WAITER_PI_LEFT = 0x28,
    GL_WAITER_TASK = 0x30,
    GL_WAITER_LOCK = 0x38,
    GL_WAITER_PRIO = 0x40,
    GL_WAITER_DEADLINE = 0x48,
    GL_WAITER_SIZE = 0x50,
};

enum {
    GL_RTMUTEX_WAIT_LOCK = 0x00,
    GL_RTMUTEX_WAITERS_RB_NODE = 0x08,
    GL_RTMUTEX_WAITERS_LEFTMOST = 0x10,
    GL_RTMUTEX_OWNER = 0x18,
};

enum {
    GL_TASK_PRIO = 0x84,
    GL_TASK_DL_DEADLINE = 0x360,
    GL_TASK_PI_LOCK = 0x86c,
    GL_TASK_PI_WAITERS_RB_NODE = 0x880,
    GL_TASK_PI_WAITERS_LEFTMOST = 0x888,
    GL_TASK_PI_TOP_TASK = 0x890,
    GL_TASK_PI_BLOCKED_ON = 0x898,
};

#define GL_RT_MUTEX_HAS_WAITERS 1UL

#define GL_MAX_RT_PRIO 100

static inline uint64_t gl_rt_mutex_owner(uint64_t owner_field)
{
    return owner_field & ~GL_RT_MUTEX_HAS_WAITERS;
}

static inline int gl_dl_prio(int prio)
{
    return prio < GL_MAX_RT_PRIO;
}

#endif
