/* Taking over from a copy that is already running.
 *
 * Loading a new ELF while an older copy runs asks the older one to stop
 * (SIGTERM, which it handles like its stop flag: pads, virtual pads, the chip
 * and the lock are let go) and waits for it to be gone. The process id comes
 * from the lock file; it is only signalled once the system's process list
 * (sysctl KERN_PROC) shows that id under PadBridge's thread name, so an
 * unrelated process that happens to have the id is never touched. There is
 * no SIGKILL: if it does not stop in time, the new copy gives up. */
#ifndef PADBRIDGE_INSTANCE_H
#define PADBRIDGE_INSTANCE_H

#include <signal.h>

#define INSTANCE_NAME     "padbridge"   /* set with thr_set_name at start-up */
#define INSTANCE_WAIT_MS  5000
#define INSTANCE_POLL_MS   200

enum {
    TAKEOVER_GONE,              /* not running (any more): the lock is free to take */
    TAKEOVER_NOT_OURS,          /* not a PadBridge, or not known to be: left alone */
    TAKEOVER_FAILED,            /* could not signal it, or the process list failed */
    TAKEOVER_TIMEOUT,           /* signalled, still running after the wait */
    TAKEOVER_STOPPED            /* this copy was asked to stop meanwhile */
};

typedef struct {
    /* 1: `pid` runs with PadBridge's name; 0: no such process, or another
     * name; -1: the process list could not be read. */
    int  (*is_padbridge)(long pid);
    /* 0 when SIGTERM was sent; -1 with errno set. */
    int  (*terminate)(long pid);
    void (*sleep_ms)(int ms);
    volatile sig_atomic_t *stop;        /* may be NULL */
} instance_ops;

/* Asks `pid` to stop and waits up to `wait_ms`, reading every `poll_ms`.
 * Returns TAKEOVER_*. */
int instance_takeover(long pid, const instance_ops *ops, int wait_ms, int poll_ms);

/* The console's process list, kill() and usleep(). On other systems
 * is_padbridge answers -1, so nothing is ever signalled. */
const instance_ops *instance_system_ops(volatile sig_atomic_t *stop);

#endif
