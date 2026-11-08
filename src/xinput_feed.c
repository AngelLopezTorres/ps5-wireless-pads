#include "xinput_feed.h"
#include "log.h"

#include <string.h>

/* XINPUT_GAMEPAD buttons. */
enum {
    XI_DPAD_UP = 0x0001, XI_DPAD_DOWN = 0x0002, XI_DPAD_LEFT = 0x0004, XI_DPAD_RIGHT = 0x0008,
    XI_START = 0x0010, XI_BACK = 0x0020,
    XI_LB = 0x0100, XI_RB = 0x0200,
    XI_A = 0x1000, XI_B = 0x2000, XI_X = 0x4000, XI_Y = 0x8000,
};

static void put16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

/* 0..255 centred on 128 to -32768..32767; `up` turns "grows downwards" round. */
static int16_t axis(uint8_t v, int up)
{
    int c = up ? 128 - (int)v : (int)v - 128;
    if (c >= 0) return (int16_t)(c * 32767 / 127 > 32767 ? 32767 : c * 32767 / 127);
    return (int16_t)(c * 256 < -32768 ? -32768 : c * 256);
}

void xfeed_encode(const pad_state *st, int slot, int connected, uint32_t seq, uint8_t out[XFEED_SIZE])
{
    unsigned b = 0;
    int w;

    memset(out, 0, XFEED_SIZE);
    memcpy(out, "PBXI", 4);
    out[4] = XFEED_VERSION;
    out[5] = (uint8_t)slot;
    out[6] = connected ? 1 : 0;
    out[7] = XFEED_SUBTYPE;
    put16(out + 8, XFEED_VID);
    put16(out + 10, XFEED_PID);
    put16(out + 12, XFEED_REVISION);
    put32(out + 26, seq);
    if (!connected || !st) return;

    if (st->buttons & PAD_CROSS)    b |= XI_A;          /* green */
    if (st->buttons & PAD_CIRCLE)   b |= XI_B;          /* red */
    if (st->buttons & PAD_TRIANGLE) b |= XI_Y;          /* yellow */
    if (st->buttons & PAD_SQUARE)   b |= XI_X;          /* blue */
    if (st->buttons & PAD_L1)       b |= XI_LB;         /* orange */
    if (st->buttons & PAD_UP)       b |= XI_DPAD_UP;    /* strum up */
    if (st->buttons & PAD_DOWN)     b |= XI_DPAD_DOWN;  /* strum down */
    if (st->buttons & PAD_LEFT)     b |= XI_DPAD_LEFT;
    if (st->buttons & PAD_RIGHT)    b |= XI_DPAD_RIGHT;
    if (st->buttons & PAD_OPTIONS)  b |= XI_START;      /* + */
    if (st->buttons & PAD_CREATE)   b |= XI_BACK;       /* - : Star Power */
    put16(out + 14, b);
    out[16] = st->l2;
    out[17] = st->r2;
    put16(out + 18, (uint16_t)axis(st->lx, 0));
    put16(out + 20, (uint16_t)axis(st->ly, 1));
    /* Whammy: 128 at rest to 255 fully down, as -32768 to 32767. */
    w = st->rx < 128 ? 0 : st->rx - 128;
    put16(out + 22, (uint16_t)(int16_t)(-32768 + w * 65535 / 127));
    put16(out + 24, 0);
}

void xfeed_init(xfeed *f, xfeed_send_fn send, void *ctx)
{
    memset(f, 0, sizeof *f);
    f->send = send;
    f->ctx = ctx;
}

static void send_slot(xfeed *f, int slot, const uint8_t *msg, long now)
{
    if (f->send) f->send(f->ctx, msg, XFEED_SIZE);
    f->slot[slot].t_sent = now;
}

void xfeed_gone(xfeed *f, int slot)
{
    uint8_t msg[XFEED_SIZE];

    if (slot < 0 || slot >= XFEED_SLOTS || !f->slot[slot].active) return;
    xfeed_encode(NULL, slot, 0, ++f->slot[slot].seq, msg);
    send_slot(f, slot, msg, f->slot[slot].t_sent);
    f->slot[slot].active = 0;
    log_line("slot %d: guitar no longer sent to XInput", slot);
}

void xfeed_state(xfeed *f, int slot, const pad_state *st, long now)
{
    uint8_t msg[XFEED_SIZE];

    if (slot < 0 || slot >= XFEED_SLOTS || !st) return;
    if (st->instrument != PAD_INSTRUMENT_GUITAR) {
        xfeed_gone(f, slot);
        return;
    }
    xfeed_encode(st, slot, 1, 0, msg);
    if (f->slot[slot].active && !memcmp(msg, f->slot[slot].last, XFEED_SIZE) &&
        now - f->slot[slot].t_sent < XFEED_REPEAT_MS)
        return;
    if (!f->slot[slot].active)
        log_line("slot %d: guitar sent to XInput listeners on 127.0.0.1:%d", slot, XFEED_PORT);
    memcpy(f->slot[slot].last, msg, XFEED_SIZE);     /* compared without the sequence */
    put32(msg + 26, ++f->slot[slot].seq);
    f->slot[slot].active = 1;
    send_slot(f, slot, msg, now);
}

void xfeed_tick(xfeed *f, long now)
{
    int i;

    for (i = 0; i < XFEED_SLOTS; i++) {
        uint8_t msg[XFEED_SIZE];
        if (!f->slot[i].active || now - f->slot[i].t_sent < XFEED_REPEAT_MS) continue;
        memcpy(msg, f->slot[i].last, XFEED_SIZE);
        put32(msg + 26, ++f->slot[i].seq);
        send_slot(f, i, msg, now);
    }
}

void xfeed_close(xfeed *f)
{
    int i;

    for (i = 0; i < XFEED_SLOTS; i++) xfeed_gone(f, i);
}
