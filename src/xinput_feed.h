/* A guitar's state for a game's XInput, sent to whoever listens on the
 * console itself (prospero-win's runtime: its `[input] net_xinput`).
 *
 * The virtual DualSense loses what makes a pad an instrument: a Windows
 * game reading XInput sees a gamepad. So for every pad that is a guitar,
 * its state also goes out as an XInput guitar, in a small fixed message,
 * to UDP 127.0.0.1:XFEED_PORT: on every change, every XFEED_REPEAT_MS while
 * nothing changes, and once more as "gone" when it leaves. Nobody listening
 * costs nothing. The message (little-endian, XFEED_SIZE bytes):
 *
 *    0  "PBXI"
 *    4  u8  version (1)
 *    5  u8  pad index (PadBridge's slot, 0..3)
 *    6  u8  flags: bit 0 connected
 *    7  u8  XInput subtype (7: guitar alternate, as the X-plorer)
 *    8  u16 vendor id   10 u16 product id   12 u16 revision
 *   14  u16 XINPUT_GAMEPAD buttons
 *   16  u8  left trigger   17 u8 right trigger
 *   18  i16 thumb lx   20 i16 thumb ly   22 i16 thumb rx   24 i16 thumb ry
 *   26  u32 sequence (per pad index, from 1)
 *   30  u16 0
 *
 * A guitar as a wired Xbox 360 X-plorer reports it: frets A B Y X LB
 * (green red yellow blue orange), strum on the d-pad, + Start, - Back
 * (Star Power), the stick on the left thumb, whammy on the right thumb's x
 * from -32768 at rest to 32767. */
#ifndef PH_XINPUT_FEED_H
#define PH_XINPUT_FEED_H

#include "pad.h"

#include <stdint.h>

#define XFEED_PORT       8097
#define XFEED_SIZE       32
#define XFEED_VERSION    1
#define XFEED_REPEAT_MS  100
#define XFEED_SLOTS      4

/* The identity a guitar is given: the RedOctane X-plorer's. */
#define XFEED_SUBTYPE    7
#define XFEED_VID        0x1430
#define XFEED_PID        0x4748
#define XFEED_REVISION   0x3122

/* Sends one message; the result is not looked at. */
typedef void (*xfeed_send_fn)(void *ctx, const uint8_t *msg, int len);

typedef struct {
    xfeed_send_fn send;
    void *ctx;
    struct {
        int active;                 /* sent as connected, not yet as gone */
        uint8_t last[XFEED_SIZE];
        uint32_t seq;
        long t_sent;
    } slot[XFEED_SLOTS];
} xfeed;

void xfeed_init(xfeed *f, xfeed_send_fn send, void *ctx);

/* A pad's newest state. Sent if it is a guitar and changed, or the repeat is
 * due; a pad that stops being a guitar (its guitar unplugged) is sent as
 * gone. */
void xfeed_state(xfeed *f, int slot, const pad_state *st, long now);

/* The pad left: "gone", once, if it was sent as a guitar. */
void xfeed_gone(xfeed *f, int slot);

/* The repeat for every guitar whose state did not change. */
void xfeed_tick(xfeed *f, long now);

/* Every guitar sent as gone (stopping, rest mode). */
void xfeed_close(xfeed *f);

/* The message for a state, without the sequence: for tests. */
void xfeed_encode(const pad_state *st, int slot, int connected, uint32_t seq, uint8_t out[XFEED_SIZE]);

#endif
