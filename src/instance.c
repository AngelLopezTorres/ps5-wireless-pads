#include "instance.h"

#include <sys/types.h>
#ifndef __linux__
#include <sys/sysctl.h>
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int instance_takeover(long pid, const instance_ops *ops, int wait_ms, int poll_ms)
{
    int waited, r;

    if (pid <= 0) return TAKEOVER_GONE;
    if (ops->is_padbridge(pid) != 1) return TAKEOVER_NOT_OURS;     /* never signal a stranger */
    if (ops->terminate(pid) != 0)
        return errno == ESRCH ? TAKEOVER_GONE : TAKEOVER_FAILED;
    for (waited = 0;; waited += poll_ms) {
        r = ops->is_padbridge(pid);
        if (r == 0) return TAKEOVER_GONE;
        if (r < 0) return TAKEOVER_FAILED;
        if (waited >= wait_ms) return TAKEOVER_TIMEOUT;
        if (ops->stop && *ops->stop) return TAKEOVER_STOPPED;
        ops->sleep_ms(poll_ms);
    }
}

/* ---- the console ------------------------------------------------------------ */

#ifndef __linux__
/* Offsets in the PS5's struct kinfo_proc, as ShadowMountPlus reads them
 * (src/main.c): ki_pid, and ki_tdname, the name thr_set_name gave the
 * process's first thread. */
#define KINFO_PID_OFFSET     72
#define KINFO_TDNAME_OFFSET 447

static int sys_is_padbridge(long pid)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    size_t size = 0;
    uint8_t *buf, *p, *end;
    int found = 0;

    if (sysctl(mib, 4, NULL, &size, NULL, 0) != 0) return -1;
    if (size == 0) return 0;
    size += size / 8;                   /* room for processes started meanwhile */
    if (!(buf = malloc(size))) return -1;
    if (sysctl(mib, 4, buf, &size, NULL, 0) != 0) {
        free(buf);
        return -1;
    }
    for (p = buf, end = buf + size; (size_t)(end - p) >= sizeof(int);) {
        int structsize;
        pid_t kpid;
        const char *name;
        size_t name_max;

        memcpy(&structsize, p, sizeof structsize);
        if (structsize <= KINFO_TDNAME_OFFSET || (size_t)structsize > (size_t)(end - p)) {
            found = -1;                 /* not the layout expected: do not guess */
            break;
        }
        memcpy(&kpid, p + KINFO_PID_OFFSET, sizeof kpid);
        name = (const char *)p + KINFO_TDNAME_OFFSET;
        name_max = (size_t)structsize - KINFO_TDNAME_OFFSET;
        p += structsize;
        if ((long)kpid != pid) continue;
        found = strnlen(name, name_max) < name_max && strcmp(name, INSTANCE_NAME) == 0;
        break;
    }
    free(buf);
    return found;
}
#else
/* Host builds: the process list is not read, so nothing is signalled. */
static int sys_is_padbridge(long pid)
{
    (void)pid;
    return -1;
}
#endif

static int sys_terminate(long pid)
{
    return kill((pid_t)pid, SIGTERM);
}

static void sys_sleep_ms(int ms)
{
    usleep((useconds_t)ms * 1000);
}

const instance_ops *instance_system_ops(volatile sig_atomic_t *stop)
{
    static instance_ops ops = { sys_is_padbridge, sys_terminate, sys_sleep_ms, NULL };

    ops.stop = stop;
    return &ops;
}
