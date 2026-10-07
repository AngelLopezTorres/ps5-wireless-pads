/* Rest mode: suspend on the way down, resume only after a steady grace
 * period, and the backoff of the tries that follow. */
#include "../src/suspend.h"
#include "../src/ps5_power.h"

#include <stdio.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

int main(void)
{
    suspend_t s;
    long t;

    printf("rest mode\n");

    suspend_init(&s);
    CHECK(s.state == SUS_ACTIVE);
    CHECK(suspend_step(&s, POWER_WORKING, 0) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_UNKNOWN, 200) == SUS_NONE);        /* unknown while active: nothing */
    CHECK(s.state == SUS_ACTIVE);
    CHECK(suspend_poll_ms(&s) == SUSPEND_POLL_MS);

    /* Going to rest: suspend once. */
    CHECK(suspend_step(&s, POWER_GOING_TO_REST, 400) == SUS_DO_SUSPEND);
    CHECK(s.state == SUS_SUSPENDED);
    CHECK(suspend_poll_ms(&s) == SUSPEND_IDLE_MS);
    CHECK(suspend_step(&s, POWER_GOING_TO_REST, 1000) == SUS_NONE); /* not twice */
    CHECK(suspend_step(&s, POWER_IN_REST, 2000) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_UNKNOWN, 3000) == SUS_NONE);       /* unknown in rest: stay */
    CHECK(s.state == SUS_SUSPENDED);

    /* Awake: the grace period first. */
    t = 10000;
    CHECK(suspend_step(&s, POWER_WORKING, t) == SUS_NONE);
    CHECK(s.state == SUS_GRACE);
    CHECK(suspend_poll_ms(&s) == SUSPEND_IDLE_MS);
    CHECK(suspend_step(&s, POWER_WORKING, t + SUSPEND_GRACE_MS - 1) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_WORKING, t + SUSPEND_GRACE_MS) == SUS_DO_RESUME);
    CHECK(s.state == SUS_ACTIVE);
    CHECK(suspend_step(&s, POWER_WORKING, t + SUSPEND_GRACE_MS + 200) == SUS_NONE);   /* not twice */

    /* Straight into rest from active. */
    CHECK(suspend_step(&s, POWER_IN_REST, 20000) == SUS_DO_SUSPEND);

    /* Back to rest during the grace period: no resume, the wait starts over. */
    t = 30000;
    CHECK(suspend_step(&s, POWER_WORKING, t) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_GOING_TO_REST, t + 1000) == SUS_NONE);
    CHECK(s.state == SUS_SUSPENDED);
    CHECK(suspend_step(&s, POWER_WORKING, t + SUSPEND_GRACE_MS) == SUS_NONE);
    CHECK(s.state == SUS_GRACE);
    CHECK(suspend_step(&s, POWER_WORKING, t + SUSPEND_GRACE_MS + 1000) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_WORKING, t + 2 * SUSPEND_GRACE_MS) == SUS_DO_RESUME);

    /* An unknown reading during the grace period also starts it over. */
    suspend_init(&s);
    CHECK(suspend_step(&s, POWER_IN_REST, 0) == SUS_DO_SUSPEND);
    CHECK(suspend_step(&s, POWER_WORKING, 1000) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_UNKNOWN, 3000) == SUS_NONE);
    CHECK(s.state == SUS_SUSPENDED);
    CHECK(suspend_step(&s, POWER_WORKING, 1000 + SUSPEND_GRACE_MS) == SUS_NONE);
    CHECK(suspend_step(&s, POWER_WORKING, 1000 + 2 * SUSPEND_GRACE_MS) == SUS_DO_RESUME);

    /* The tries after rest back off, and the wait stops growing. */
    CHECK(suspend_backoff_ms(1) == RESUME_BACKOFF_MS);
    CHECK(suspend_backoff_ms(2) == 2 * RESUME_BACKOFF_MS);
    CHECK(suspend_backoff_ms(3) == 4 * RESUME_BACKOFF_MS);
    CHECK(suspend_backoff_ms(4) == RESUME_BACKOFF_MAX);
    CHECK(suspend_backoff_ms(50) == RESUME_BACKOFF_MAX);
    CHECK(suspend_backoff_ms(0) == RESUME_BACKOFF_MS);

    printf(g_fail ? "%d check(s) failed\n" : "all rest mode checks passed\n", g_fail);
    return g_fail != 0;
}
