
#include "common.h"

int gl_run_all(void);

extern "C" __attribute__((visibility("default")))
int gl_payload_main(void)
{
    return gl_run_all();
}

__attribute__((constructor))
static void gl_payload_ctor(void)
{
    (void)gl_payload_main();
}
