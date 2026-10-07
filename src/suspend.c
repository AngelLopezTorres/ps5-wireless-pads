#include "suspend.h"
#include "ps5_power.h"

void suspend_init(suspend_t *s)
{
    s->state = SUS_ACTIVE;
    s->since = 0;
}

int suspend_step(suspend_t *s, int power, long now)
{
    int rest = power == POWER_GOING_TO_REST || power == POWER_IN_REST;

    switch (s->state) {
    case SUS_ACTIVE:
        if (!rest) return SUS_NONE;
        s->state = SUS_SUSPENDED;
        return SUS_DO_SUSPEND;
    case SUS_SUSPENDED:
        if (power != POWER_WORKING) return SUS_NONE;
        s->state = SUS_GRACE;
        s->since = now;
        return SUS_NONE;
    default:                            /* SUS_GRACE */
        if (power != POWER_WORKING) {   /* rest again, or not sure: wait again */
            s->state = SUS_SUSPENDED;
            return SUS_NONE;
        }
        if (now - s->since < SUSPEND_GRACE_MS) return SUS_NONE;
        s->state = SUS_ACTIVE;
        return SUS_DO_RESUME;
    }
}

int suspend_poll_ms(const suspend_t *s)
{
    return s->state == SUS_ACTIVE ? SUSPEND_POLL_MS : SUSPEND_IDLE_MS;
}

long suspend_backoff_ms(int attempt)
{
    long ms = RESUME_BACKOFF_MS;

    while (--attempt > 0 && ms < RESUME_BACKOFF_MAX) ms *= 2;
    return ms < RESUME_BACKOFF_MAX ? ms : RESUME_BACKOFF_MAX;
}
