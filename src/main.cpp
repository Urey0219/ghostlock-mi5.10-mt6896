#include "common.h"

int gl_run_all(void);

static int run_child(void)
{
    int rc = gl_run_all();

    if (rc == 0)
        return 0;

    fputs("failed\n", stderr);
    return 3;
}

int main(void)
{
    pid_t child;

    child = fork();
    if (child < 0)
        return 1;

    if (child == 0) {
        int rc = run_child();

        gl_drain_or_park();
        _exit(rc);
    }

    for (;;) {
        int status = 0;
        pid_t r = waitpid(child, &status, 0);

        if (r < 0) {
            if (errno == EINTR)
                continue;
            return 1;
        }
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            break;
        return 1;
    }

    return 0;
}
