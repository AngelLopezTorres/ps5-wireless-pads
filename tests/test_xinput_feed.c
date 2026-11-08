/* The guitar's XInput message: layout, mapping, repeat and "gone". */
#include "../src/xinput_feed.h"
#include "../src/log.h"

#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

static uint8_t g_msg[16][XFEED_SIZE];
static int g_sent;
static void capture(void *ctx, const uint8_t *msg, int len)
{
    (void)ctx;
    if (len == XFEED_SIZE && g_sent < 16) memcpy(g_msg[g_sent], msg, XFEED_SIZE);
    g_sent++;
}

static unsigned u16(const uint8_t *p) { return p[0] | p[1] << 8; }
static int s16(const uint8_t *p) { return (int16_t)u16(p); }
static uint32_t u32(const uint8_t *p) { return u16(p) | (uint32_t)u16(p + 2) << 16; }

static pad_state guitar(void)
{
    pad_state st;
    pad_state_reset(&st);
    st.instrument = PAD_INSTRUMENT_GUITAR;
    return st;
}

int main(void)
{
    xfeed f;
    pad_state st = guitar();
    uint8_t m[XFEED_SIZE];

    printf("the message: header, identity, mapping\n");
    st.buttons = PAD_CROSS | PAD_CIRCLE | PAD_TRIANGLE | PAD_SQUARE | PAD_L1 | PAD_UP | PAD_OPTIONS | PAD_CREATE;
    st.lx = 255; st.ly = 0; st.rx = 255;
    xfeed_encode(&st, 2, 1, 7, m);
    CHECK(!memcmp(m, "PBXI", 4) && m[4] == 1 && m[5] == 2 && m[6] == 1 && m[7] == 7);
    CHECK(u16(m + 8) == 0x1430 && u16(m + 10) == 0x4748 && u16(m + 12) == 0x3122);
    CHECK(u16(m + 14) == (0x1000 | 0x2000 | 0x8000 | 0x4000 | 0x0100 | 0x0001 | 0x0010 | 0x0020));
    CHECK(s16(m + 18) == 32767 && s16(m + 20) == 32767);       /* right, up */
    CHECK(s16(m + 22) == 32767);                                 /* whammy fully down */
    CHECK(s16(m + 24) == 0 && u32(m + 26) == 7 && m[30] == 0 && m[31] == 0);
    st.buttons = PAD_DOWN;
    st.lx = 0; st.ly = 255; st.rx = 128;
    xfeed_encode(&st, 0, 1, 1, m);
    CHECK(u16(m + 14) == 0x0002);                                /* strum down */
    CHECK(s16(m + 18) == -32768 && s16(m + 20) == -32512);
    CHECK(s16(m + 22) == -32768);                                /* whammy at rest */
    st.lx = st.ly = 128;
    xfeed_encode(&st, 0, 1, 1, m);
    CHECK(s16(m + 18) == 0 && s16(m + 20) == 0);
    xfeed_encode(NULL, 1, 0, 3, m);
    CHECK(m[5] == 1 && m[6] == 0 && u16(m + 14) == 0 && u32(m + 26) == 3);

    printf("sent on change, repeated while still, gone once\n");
    xfeed_init(&f, capture, NULL);
    st = guitar();
    xfeed_state(&f, 0, &st, 1000);
    CHECK(g_sent == 1 && g_msg[0][6] == 1 && u32(g_msg[0] + 26) == 1);
    xfeed_state(&f, 0, &st, 1010);                               /* no change, not due */
    CHECK(g_sent == 1);
    xfeed_tick(&f, 1050);
    CHECK(g_sent == 1);
    xfeed_tick(&f, 1100);                                        /* due */
    CHECK(g_sent == 2 && u32(g_msg[1] + 26) == 2 && !memcmp(g_msg[1], g_msg[0], 26));
    st.buttons = PAD_CROSS;
    xfeed_state(&f, 0, &st, 1110);                               /* changed: at once */
    CHECK(g_sent == 3 && u16(g_msg[2] + 14) == 0x1000 && u32(g_msg[2] + 26) == 3);
    st.instrument = PAD_INSTRUMENT_NONE;                         /* guitar unplugged */
    xfeed_state(&f, 0, &st, 1120);
    CHECK(g_sent == 4 && g_msg[3][6] == 0 && u32(g_msg[3] + 26) == 4);
    xfeed_state(&f, 0, &st, 1130);
    xfeed_tick(&f, 2000);
    xfeed_gone(&f, 0);
    CHECK(g_sent == 4);                                          /* nothing for a plain pad */

    printf("a plain pad is never sent; closing sends gone for each guitar\n");
    g_sent = 0;
    xfeed_init(&f, capture, NULL);
    pad_state_reset(&st);
    xfeed_state(&f, 1, &st, 0);
    CHECK(g_sent == 0);
    st = guitar();
    xfeed_state(&f, 1, &st, 0);
    xfeed_state(&f, 3, &st, 0);
    xfeed_state(&f, 4, &st, 0);                                  /* no such slot */
    xfeed_state(&f, -1, &st, 0);
    CHECK(g_sent == 2);
    xfeed_close(&f);
    CHECK(g_sent == 4 && g_msg[2][5] == 1 && g_msg[2][6] == 0 && g_msg[3][5] == 3 && g_msg[3][6] == 0);
    xfeed_close(&f);
    CHECK(g_sent == 4);

    if (g_fail) { printf("%d xinput feed checks failed\n", g_fail); return 1; }
    printf("all xinput feed checks passed\n");
    return 0;
}
