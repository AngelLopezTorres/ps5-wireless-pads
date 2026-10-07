/* Rest mode without stopping: when to let go of the hardware and when to take
 * it back.
 *
 * The main loop feeds it the console's power state (ps5_power.h) and the
 * time; it answers with what to do. Going to rest, or in it: suspend (close
 * Bluetooth, the virtual pads and the menu). Back to working: wait a grace
 * period, the state reading "working" at every poll, then resume (open them
 * again). An unknown reading while waiting starts the wait over: the chip is
 * only touched again once the console says plainly that it is awake.
 *
 * Pure logic, no system calls, so it runs in the host tests. */
#ifndef PADBRIDGE_SUSPEND_H
#define PADBRIDGE_SUSPEND_H

#define SUSPEND_GRACE_MS    5000    /* working this long before resuming */
#define SUSPEND_POLL_MS      200    /* power state reads while active */
#define SUSPEND_IDLE_MS      750    /* ... and while suspended */
#define RESUME_TRIES           5    /* Bluetooth / menu tries after rest */
#define RESUME_BACKOFF_MS   2000    /* first wait between them, doubling */
#define RESUME_BACKOFF_MAX 16000

enum { SUS_ACTIVE, SUS_SUSPENDED, SUS_GRACE };          /* where it is */
enum { SUS_NONE, SUS_DO_SUSPEND, SUS_DO_RESUME };       /* what to do now */

typedef struct {
    int  state;
    long since;                 /* SUS_GRACE: when "working" was first read */
} suspend_t;

void suspend_init(suspend_t *s);

/* One power reading (POWER_*) at `now` ms. Returns SUS_*. */
int  suspend_step(suspend_t *s, int power, long now);

/* How long to wait before reading the power state again. */
int  suspend_poll_ms(const suspend_t *s);

/* The wait before try `attempt` + 1 after rest (attempt >= 1): 2, 4, 8, 16,
 * 16 ... seconds. */
long suspend_backoff_ms(int attempt);

#endif
