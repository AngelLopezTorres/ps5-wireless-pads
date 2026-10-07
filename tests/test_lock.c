/* The instance lock: live, dead, from another boot, and in the old format;
 * its owner's id, and taking over from it. */
#include "../src/instance.h"
#include "../src/lock.h"

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

#define LOCK "build/test_lock.lock"

static void write_lock(const char *text)
{
    FILE *f = fopen(LOCK, "w");
    fputs(text, f);
    fclose(f);
}

static pid_t dead_pid(void)
{
    pid_t p = fork();
    if (p == 0) _exit(0);
    waitpid(p, NULL, 0);
    return p;
}

/* ---- a pretend process list for the takeover ---- */

static long f_pid;              /* the process the fake list knows */
static int  f_name_ok;          /* ... runs as PadBridge */
static int  f_list_fails;       /* the list cannot be read */
static int  f_dies_after;       /* polls before it exits once signalled; -1 never */
static int  f_term_errno;       /* kill() fails with this, 0 = succeeds */
static int  f_terms, f_polls, f_slept;
static volatile sig_atomic_t f_stop;
static int  f_stop_after;       /* set f_stop at this sleep, -1 never */

static int fake_is_padbridge(long pid)
{
    if (f_list_fails) return -1;
    if (pid != f_pid || !f_name_ok) return 0;
    if (f_terms && f_dies_after >= 0 && f_polls++ >= f_dies_after) return 0;
    return 1;
}

static int fake_terminate(long pid)
{
    (void)pid;
    f_terms++;
    if (f_term_errno) {
        errno = f_term_errno;
        return -1;
    }
    return 0;
}

static void fake_sleep(int ms)
{
    f_slept += ms;
    if (f_stop_after >= 0 && f_slept >= f_stop_after) f_stop = 1;
}

static const instance_ops f_ops = { fake_is_padbridge, fake_terminate, fake_sleep, &f_stop };

static void fake(long pid, int name_ok, int dies_after)
{
    f_pid = pid;
    f_name_ok = name_ok;
    f_dies_after = dies_after;
    f_list_fails = f_term_errno = 0;
    f_terms = f_polls = f_slept = 0;
    f_stop = 0;
    f_stop_after = -1;
}

static void takeover_checks(void)
{
    printf("taking over from a running copy\n");

    fake(4242, 1, 2);                               /* PadBridge, exits after the signal */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_GONE);
    CHECK(f_terms == 1);                            /* one SIGTERM */
    CHECK(f_slept < 5000);

    fake(4242, 0, 0);                               /* the id runs as something else */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_NOT_OURS);
    CHECK(f_terms == 0);                            /* never signalled */

    fake(4242, 1, 0);
    f_list_fails = 1;                               /* the list cannot be read */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_NOT_OURS);
    CHECK(f_terms == 0);

    fake(4242, 1, 0);
    CHECK(instance_takeover(0, &f_ops, 5000, 200) == TAKEOVER_GONE);    /* no id */
    CHECK(instance_takeover(-1, &f_ops, 5000, 200) == TAKEOVER_GONE);
    CHECK(f_terms == 0);

    fake(4242, 1, -1);                              /* ignores SIGTERM */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_TIMEOUT);
    CHECK(f_terms == 1);                            /* no second signal, no SIGKILL */
    CHECK(f_slept >= 5000 && f_slept <= 5200);      /* bounded */

    fake(4242, 1, 0);
    f_term_errno = ESRCH;                           /* gone between the check and the signal */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_GONE);

    fake(4242, 1, 0);
    f_term_errno = EPERM;                           /* not allowed */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_FAILED);

    fake(4242, 1, -1);
    f_stop_after = 600;                             /* asked to stop while waiting */
    CHECK(instance_takeover(4242, &f_ops, 5000, 200) == TAKEOVER_STOPPED);
    CHECK(f_slept < 5000);
}

int main(void)
{
    char text[64];
    long boot = lock_boot_time();
    struct timeval old[2];

    printf("the instance lock\n");
    CHECK(boot > 1000000000L);                      /* the boot time is readable */
    unlink(LOCK);

    CHECK(lock_take(LOCK));                         /* free: taken */
    {
        FILE *f = fopen(LOCK, "r");
        long pid = 0, b = 0;
        CHECK(f && fscanf(f, "%ld %ld", &pid, &b) == 2 && pid == (long)getpid() && b == boot);
        if (f) fclose(f);
    }
    lock_release();
    CHECK(access(LOCK, F_OK) != 0);                 /* released: gone */

    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot);
    write_lock(text);
    CHECK(!lock_take(LOCK));                        /* a live owner of this boot: refused */
    CHECK(access(LOCK, F_OK) == 0);                 /* and its lock is left alone */

    snprintf(text, sizeof text, "%d %ld\n", (int)dead_pid(), boot);
    write_lock(text);
    CHECK(lock_take(LOCK));                         /* its process is gone: stale */
    lock_release();

    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot - 100);
    write_lock(text);
    CHECK(lock_take(LOCK));                         /* a live id, but from another boot: stale */
    lock_release();

    snprintf(text, sizeof text, "%d\n", (int)getppid());
    write_lock(text);                               /* old format, written just now */
    CHECK(!lock_take(LOCK));                        /* live: refused */
    old[0].tv_sec = old[1].tv_sec = boot - 3600;    /* ... and the same file from before the boot */
    old[0].tv_usec = old[1].tv_usec = 0;
    utimes(LOCK, old);
    CHECK(lock_take(LOCK));                         /* stale by its age */
    lock_release();

    /* The owner's id: only for a live lock of this boot. */
    unlink(LOCK);
    CHECK(lock_owner(LOCK) == 0);                   /* no lock */
    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot);
    write_lock(text);
    CHECK(lock_owner(LOCK) == (long)getppid());     /* live */
    snprintf(text, sizeof text, "%d %ld\n", (int)dead_pid(), boot);
    write_lock(text);
    CHECK(lock_owner(LOCK) == 0);                   /* gone */
    snprintf(text, sizeof text, "%d %ld\n", (int)getppid(), boot - 100);
    write_lock(text);
    CHECK(lock_owner(LOCK) == 0);                   /* another boot */
    snprintf(text, sizeof text, "%d %ld\n", (int)getpid(), boot);
    write_lock(text);
    CHECK(lock_owner(LOCK) == 0);                   /* ourselves */

    write_lock("garbage\n");
    CHECK(lock_take(LOCK));                         /* unreadable: stale */
    lock_release();
    write_lock("");
    CHECK(lock_take(LOCK));                         /* empty: stale */
    lock_release();

    takeover_checks();

    printf(g_fail ? "%d check(s) failed\n" : "all lock checks passed\n", g_fail);
    return g_fail != 0;
}
