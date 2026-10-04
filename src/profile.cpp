#include "common.h"
#include "offsets.h"

static const struct gl_offsets k_profile_136 = {
    .kernel_base = KIMAGE_TEXT_BASE,

    .waiter_off_task = GL_WAITER_TASK,
    .waiter_off_lock = GL_WAITER_LOCK,
    .waiter_off_prio = GL_WAITER_PRIO,
    .waiter_off_deadline = GL_WAITER_DEADLINE,
    .waiter_size = GL_WAITER_SIZE,

    .rtmutex_off_wait_lock = GL_RTMUTEX_WAIT_LOCK,
    .rtmutex_off_waiters_rb_node = GL_RTMUTEX_WAITERS_RB_NODE,
    .rtmutex_off_waiters_leftmost = GL_RTMUTEX_WAITERS_LEFTMOST,
    .rtmutex_off_owner = GL_RTMUTEX_OWNER,

    .task_off_prio = GL_TASK_PRIO,
    .task_off_dl_deadline = GL_TASK_DL_DEADLINE,
    .task_off_pi_lock = GL_TASK_PI_LOCK,
    .task_off_pi_waiters_rb_node = GL_TASK_PI_WAITERS_RB_NODE,
    .task_off_pi_waiters_leftmost = GL_TASK_PI_WAITERS_LEFTMOST,
    .task_off_pi_top_task = GL_TASK_PI_TOP_TASK,
    .task_off_pi_blocked_on = GL_TASK_PI_BLOCKED_ON,

    .task_off_cred = 0x780,
    .task_off_real_cred = 0x778,

    .task_off_comm = 0x790,
    .task_off_tasks = 0x448,
    .task_off_pid = 0x470,
    .task_off_tgid = 0x474,
    .task_off_mm = 0x518,

    .cred_off_uid = 0x04,
    .cred_off_gid = 0x08,
    .cred_off_euid = 0x14,
    .cred_off_egid = 0x18,
    .cred_off_suid = 0x0c,
    .cred_off_sgid = 0x10,
    .cred_off_fsuid = 0x1c,
    .cred_off_fsgid = 0x20,
    .cred_off_cap_inheritable = 0x28,
    .cred_off_cap_permitted = 0x30,
    .cred_off_cap_effective = 0x38,
    .cred_off_cap_bset = 0x40,
    .cred_off_cap_ambient = 0x48,
    .cred_off_security = 0x78,
    .cred_off_user_ns = 0x80,

    .sym_remove_waiter = 0xffffffc0101e7204ULL,
    .sym_rt_mutex_adjust_prio_chain = 0xffffffc0101e78bcULL,

    .sym_chain_precheck = 0xffffffc0101e923cULL,
    .sym_task_blocks_on_rt_mutex = 0xffffffc0101e6a80ULL,
    .sym_try_to_take_rt_mutex = 0xffffffc0101e6528ULL,
    .sym_mark_wakeup_next_waiter = 0xffffffc0101e6144ULL,
    .sym_futex_lock_pi = 0xffffffc010290a28ULL,
    .sym_futex_unlock_pi = 0xffffffc0102915d0ULL,

    .sym_futex_wait_requeue_pi = 0xffffffc010292110ULL,
    .sym_do_futex = 0xffffffc01028d7d4ULL,
    .sym___arm64_sys_futex = 0xffffffc010297068ULL,
    .sym___set_task_comm = 0xffffffc010555178ULL,
    .sym_init_cred = 0xffffffc012790930ULL,
    .sym_init_task = 0xffffffc01277bf80ULL,

    .sym_zero_page = 0xffffffc012970000ULL,

    .sym_saved_command_line = 0xffffffc012972018ULL,

    .sym_binder_devices_param = 0xffffffc0128cb4a0ULL,
    .sym_modprobe_path = 0xffffffc012790cc8ULL,
    .sym_core_pattern = 0xffffffc01283efc8ULL,
    .sym_selinux_enforcing = 0xffffffc012724c3cULL,

    .sym_selinux_state = 0xffffffc012a25b90ULL,
    .sym_sysctl_bootid = 0xffffffc012a3f345ULL,
    .sym_nfulnl_logger = 0xffffffc012771450ULL,
    .sym_init_user_ns = 0xffffffc01278f548ULL,

    .sym_ptmx_fops = 0xffffffc0122e1408ULL,
};

const struct gl_offsets *gl_profile(void)
{
    return &k_profile_136;
}
