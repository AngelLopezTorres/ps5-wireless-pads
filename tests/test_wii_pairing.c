/* End-to-end PIN selection through discovery, name lookup and authentication. */
#include "sim.h"
#include "../src/host.h"
#include "../src/log.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int failed;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); failed++; } } while (0)
static void check_pair(const char *name, int no_name, int name_status, int legacy, int wii, int no_host_address,
                       int unanswered_pages)
{
    static const unsigned char host_pin[6] = {0x66, 0, 0x44, 0x33, 0x22, 0x11};
    sim_pad pad = {0};
    host_events events = {0};
    unsigned char pin[23];
    sim_t *sim;
    host_t *host;
    long end;
    unlink("build/test_wii_pads.db");
    pad.mode = SIM_WAIT_PAIRING;
    pad.addr[0] = 0xfd; pad.addr[1] = 0x4e; pad.addr[2] = 0x7e;
    pad.addr[3] = 0xaa; pad.addr[4] = 0x22;
    pad.cod = 0x002504;
    pad.vid = legacy == 1 ? 0x057e : 0x054c;
    pad.pid = legacy == 1 ? 0x0306 : 0x05c4;
    pad.remote_name = name; pad.no_name_reply = no_name;
    pad.no_host_address = no_host_address;
    pad.name_status = name_status; pad.legacy_pin = legacy;
    pad.unanswered_pages = unanswered_pages;
    sim = sim_new(&pad);
    host = host_open(sim_hci(sim), "build/test_wii_pads.db", &events);
    CHECK(host != NULL);
    if (!host) { sim_free(sim); return; }
    host_pair(host, 60);
    /* Long enough for a cancelled name request (3 s + 1 s) or one page
     * left unanswered (20 s), then a fresh discovery and pairing. */
    end = sim_now() + 45000;
    while (sim_now() < end && (no_host_address || sim_auth_successes(sim) == 0)) host_poll(host, 5);
    if (no_host_address) CHECK(sim_auth_successes(sim) == 0);
    else CHECK(sim_auth_successes(sim) >= 1);
    if (legacy && !no_host_address) {
        CHECK(sim_pin_replies(sim, pin) >= 1);
        CHECK(!memcmp(pin, pad.addr, 6));
        CHECK(pin[6] == (wii ? 6 : 4));
        CHECK(!memcmp(pin + 7, wii ? host_pin : (const unsigned char *)"0000", wii ? 6 : 4));
        for (int i = 7 + (wii ? 6 : 4); i < 23; i++) CHECK(pin[i] == 0);
    } else CHECK(sim_pin_replies(sim, pin) == 0);
    /* Never page over a name request or a page still running. */
    CHECK(sim_refused(sim) == 0);
    /* The name is asked over the link, never by paging the pad for it. */
    CHECK(sim_names(sim) >= 1);
    CHECK(sim_names_unlinked(sim) == 0);
    if (no_name) CHECK(sim_name_cancels(sim) == 2);     /* asked twice */
    else CHECK(sim_name_cancels(sim) == 0);
    CHECK(sim_page_cancels(sim) == unanswered_pages);
    host_close(host); sim_free(sim);
    unlink("build/test_wii_pads.db");
}
typedef struct { int connects, states; pad_info info; pad_state last; long t_ready; int inq_at_ready; } seen;
static sim_t *g_sim;                    /* for on_connect: inquiries so far */
static void on_connect(void *ud, int slot, const pad_info *info)
{
    seen *s = ud; (void)slot; s->connects++; s->info = *info; s->t_ready = sim_now();
    s->inq_at_ready = g_sim ? sim_inquiries(g_sim) : 0;
}
static void on_state(void *ud, int slot, const pad_state *st)
{
    seen *s = ud; (void)slot; s->states++; s->last = *st;
}
/* After red-Sync pairing, the remote is set up as a Wii Remote straight
 * away: no SDP (its name gave its ids), the extension's id read, and its
 * guitar's frets and strum reaching the pad state. wii: 1 guitar, 2 none.
 * trouble: 1 the answer to the first HID channel request is lost and a
 * repeat with the same cid ignored; 2 channel requests answered "pending"
 * for 4 s; 3 the first page times out, 4 the first two do: paged again at
 * once, first with R2 and no clock offset, then with the inquiry's; 5 the
 * Encryption Change event is lost: asked again, channels opened meanwhile;
 * 6 the name's answer is lost: asked again, so the Wii PIN still applies;
 * 7 encryption takes 4 s to start and the chip holds the channel request
 * meanwhile: asked once, not again under another cid the remote refuses;
 * 8 the first control-channel request is answered after 3 s, past the
 * wait: the late answer is taken, and the second request's refusal does
 * not undo it. Both 7 and 8 refuse a second request, as the remote did;
 * 9 the chip's page scan is as busy as the PS5's system sets it, under
 * which pages time out: turned down while the remote is linked, put back
 * on close; 10 the chip sends one packet every 500 ms: after one Data
 * Buffer Overflow PadBridge waits for completed-packet reports (the
 * setup may still be going round, so its last output is not checked);
 * 11 reconnecting, the remote's own control-channel request is lost and
 * it refuses PadBridge's: the link is dropped after the second refusal
 * and the remote is ready within 9 s of switching on, not after its own
 * 10 s give-up. */
static void check_wii_ready(int wii, int trouble)
{
    unsigned mode, offset;
    sim_pad pad = {0};
    seen s = {0};
    host_events events = { on_connect, on_state, NULL, &s, NULL };
    sim_t *sim;
    host_t *host;
    long end;
    unlink("build/test_wii_pads.db");
    pad.mode = SIM_WAIT_PAIRING;
    pad.addr[0] = 0xfd; pad.addr[1] = 0x4e; pad.addr[2] = 0x7e;
    pad.addr[3] = 0xaa; pad.addr[4] = 0x22;
    pad.cod = 0x002504;
    pad.vid = 0x057e; pad.pid = 0x0306;
    pad.remote_name = "Nintendo RVL-CNT-01";
    pad.legacy_pin = 1;
    pad.wii = wii;
    pad.lose_first_hid_rsp = pad.ignore_repeat_cid = trouble == 1;
    pad.pending_ms = trouble == 2 ? 4000 : 0;
    pad.page_timeouts = trouble == 3 ? 1 : trouble == 4 ? 2 : 0;
    pad.lose_event = trouble == 5 ? 0x08 : trouble == 6 ? 0x07 : 0;
    pad.enc_delay_ms = trouble == 7 ? 4000 : 0;
    pad.late_rsp_ms = trouble == 8 ? 3000 : 0;
    pad.refuse_second_req = trouble == 7 || trouble == 8;
    pad.busy_page_scan = trouble == 9;
    pad.tx_ms = trouble == 10 ? 500 : 0;
    pad.lose_own_req = trouble == 11;
    pad.needs_output = 1;
    pad.basic[0] = 0x30; pad.basic[2] = 0x08; pad.basic_len = 3;            /* A */
    pad.report[0] = 0x32; pad.report[2] = 0x08;
    pad.report[3] = 0x20; pad.report[4] = 0x20; pad.report[5] = 0x0F; pad.report[6] = 0x10;
    pad.report[7] = (unsigned char)~0x40;                                   /* strum down */
    pad.report[8] = (unsigned char)~0x10;                                   /* green */
    pad.report_len = 11;
    sim = sim_new(&pad);
    g_sim = sim;
    host = host_open(sim_hci(sim), "build/test_wii_pads.db", &events);
    CHECK(host != NULL);
    if (!host) { sim_free(sim); return; }
    host_pair(host, 60);
    end = sim_now() + 20000 + 15000 * pad.page_timeouts;
    while (sim_now() < end) host_poll(host, 5);
    CHECK(sim_pages(sim) == 1 + pad.page_timeouts);
    sim_last_page(sim, &mode, &offset);
    if (trouble == 3) CHECK(mode == 2 && offset == 0);
    else CHECK(offset & 0x8000);
    if (trouble == 5 || trouble == 7) CHECK(sim_enc_requests(sim) >= 2);
    else CHECK(sim_enc_requests(sim) == 1);
    if (trouble == 7) CHECK(sim_ctrl_reqs(sim) == 1 && sim_refused_chans(sim) == 0);
    if (trouble == 8) CHECK(sim_ctrl_reqs(sim) >= 2 && sim_refused_chans(sim) >= 1);
    {
        unsigned iv, win, type;
        CHECK(sim_busy_pages(sim) == 0);
        sim_page_scan(sim, &iv, &win, &type);
        CHECK(iv == 0x0800 && win == 0x0012 && type == 0);    /* down while linked, or never touched */
    }
    if (trouble == 6) CHECK(sim_names(sim) == 2 && sim_name_cancels(sim) == 0);
    CHECK(s.connects == 1);
    CHECK(s.info.vid == 0x057e && s.info.pid == 0x0306);
    CHECK(s.info.profile && !strcmp(s.info.profile, "Wii Remote"));
    CHECK(sim_sdp_queries(sim) == 0);
    CHECK(s.states > 0);
    if (wii == 1) CHECK(s.last.buttons == (PAD_CROSS | PAD_DOWN));
    else CHECK(s.last.buttons == PAD_CROSS);
    if (trouble != 10) CHECK(sim_last_output(sim, (unsigned char[32]){0}, 32) == 4);    /* 0xA2 0x12 0x04 0x32 */
    /* Paired: the search stops, no inquiry after it. */
    CHECK(sim_last_inquiry_at(sim) < s.t_ready);
    if (trouble == 10) CHECK(sim_overflows(sim) <= 1);
    if (trouble != 10) {
        /* The player number: the remote's LEDs (report 0x11, LED 2 is 0x20). */
        unsigned char o[32];
        CHECK(host_set_player(host, 0, 2) == 1);
        CHECK(sim_last_output(sim, o, 32) == 3 && o[0] == 0xA2 && o[1] == 0x11 && o[2] == 0x20);
        CHECK(host_set_player(host, 0, 5) == 0 && host_set_player(host, 1, 1) == 0);
    }
    if (trouble == 11) {
        long t_on;
        sim_disconnect(sim);
        sim_set_mode(sim, SIM_RECONNECT);
        for (end = sim_now() + 500; sim_now() < end;) host_poll(host, 5);
        t_on = sim_now();
        sim_power_on(sim);
        for (end = sim_now() + 20000; sim_now() < end && s.connects < 2;) host_poll(host, 5);
        CHECK(s.connects == 2);
        CHECK(sim_host_disconnects(sim) == 1);
        CHECK(s.t_ready - t_on < 9000);
    }
    if (trouble == 0) {
        /* Switched off, then on again by a button while another pairing
         * window searches: the running inquiry is cancelled and none is
         * started until the remote is ready again. */
        int inq, cancels;
        sim_disconnect(sim);
        sim_set_mode(sim, SIM_RECONNECT);
        for (end = sim_now() + 500; sim_now() < end;) host_poll(host, 5);
        host_pair(host, 60);
        for (end = sim_now() + 500; sim_now() < end;) host_poll(host, 5);
        inq = sim_inquiries(sim);
        cancels = sim_inquiry_cancels(sim);
        CHECK(inq >= 2);                            /* searching again */
        sim_power_on(sim);
        for (end = sim_now() + 10000; sim_now() < end && s.connects < 2;) host_poll(host, 5);
        CHECK(s.connects == 2);
        CHECK(sim_inquiry_cancels(sim) > cancels);  /* the search paused */
        CHECK(s.inq_at_ready == inq);               /* none while it connected */
    }
    host_close(host);
    if (trouble == 9) {                 /* the system's page scan, put back on close */
        unsigned iv, win, type;
        sim_page_scan(sim, &iv, &win, &type);
        CHECK(iv == 0x029B && win == 0x014D && type == 1);
    }
    sim_free(sim);
    g_sim = NULL;
    unlink("build/test_wii_pads.db");
}
/* The chip's settings, the system's own connections and a summary of the
 * chip's traffic reach the log. */
static void check_chip_log(void)
{
    static const char *want[] = {
        "controller: page timeout 8192 slots (5120 ms)",
        "controller: page scan every 1280 ms for 11 ms",
        "controller: inquiry scan every 1280 ms for 11 ms",
        "controller: page scan type 0 (standard)",
        "controller: inquiry mode 2 (extended)",
        "chip: connection request from ", ", class 002508, link type 1 (not ours)",
        "chip: connection complete for ", ", status 0x3e, handle 0x00b, link type 1 (no link of ours)",
        "chip: handle 0x00b disconnected, status 0x16, reason 0x13 (no link of ours)",
        "chip, last 5 s: events ",
        " 04:1 ", " 05:1 ",
        "; ACL ours 0, others 1;",
        " fc6f:0/1",                    /* a command of the system's, answered to it */
        " 0401:1/1",                    /* our inquiry */
    };
    static const unsigned char other[6] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
    unsigned char e[16], data[4] = { 0, 0, 0x40, 0 };
    sim_pad pad = {0};
    sim_t *sim;
    host_t *host;
    char text[16384];
    size_t n;
    FILE *f;
    long end;

    unlink("build/test_wii_pads.db");
    unlink("build/test_chip.log");
    pad.mode = SIM_RECONNECT;           /* stays off: only others' traffic */
    pad.addr[0] = 0xfd; pad.addr[1] = 0x4e;
    log_open("build/test_chip.log");
    sim = sim_new(&pad);
    host = host_open(sim_hci(sim), "build/test_wii_pads.db", NULL);
    CHECK(host != NULL);
    if (!host) { log_close(); sim_free(sim); return; }
    host_pair(host, 30);
    memcpy(e, other, 6); e[6] = 0x08; e[7] = 0x25; e[8] = 0x00; e[9] = 1;
    sim_inject_event(sim, 0x04, e, 10);                 /* Connection Request */
    memset(e, 0, sizeof e);
    e[0] = 0x3E; e[1] = 0x0B; memcpy(e + 3, other, 6); e[9] = 1;
    sim_inject_event(sim, 0x03, e, 11);                 /* Connection Complete */
    e[0] = 0x16; e[1] = 0x0B; e[2] = 0x00; e[3] = 0x13;
    sim_inject_event(sim, 0x05, e, 4);                  /* Disconnection Complete */
    e[0] = 1; e[1] = 0x6F; e[2] = 0xFC; e[3] = 0;
    sim_inject_event(sim, 0x0E, e, 4);                  /* the system's vendor command */
    sim_inject_acl(sim, 0x40, data, 4);              /* someone's link's data */
    end = sim_now() + 6000;
    while (sim_now() < end) host_poll(host, 5);
    host_close(host); sim_free(sim);
    log_close();
    f = fopen("build/test_chip.log", "r");
    CHECK(f != NULL);
    if (!f) return;
    n = fread(text, 1, sizeof text - 1, f);
    text[n] = 0;
    fclose(f);
    for (size_t i = 0; i < sizeof want / sizeof want[0]; i++)
        if (!strstr(text, want[i])) { printf("FAIL: log lacks \"%s\"\n", want[i]); failed++; }
    unlink("build/test_wii_pads.db");
    unlink("build/test_chip.log");
}
int main(void)
{
    check_chip_log();
    check_wii_ready(1, 0);
    check_wii_ready(2, 0);
    check_wii_ready(1, 1);
    check_wii_ready(1, 2);
    check_wii_ready(1, 3);
    check_wii_ready(1, 4);
    check_wii_ready(1, 5);
    check_wii_ready(1, 6);
    check_wii_ready(1, 7);
    check_wii_ready(1, 8);
    check_wii_ready(1, 9);
    check_wii_ready(1, 10);
    check_wii_ready(1, 11);
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 0, 0);
    check_pair("Nintendo RVL-CNT-01-TR", 0, 0, 1, 1, 0, 0);
    check_pair("Nintendo RVL-CNT-01-UC", 0, 0, 2, 0, 0, 0);
    check_pair("Other legacy controller", 0, 0, 2, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 1, 0, 2, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 0, 1, 2, 0, 0, 0);
    check_pair("DualShock 4", 0, 0, 0, 0, 0, 0);
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 1, 0);
    /* The first page goes unanswered: cancelled at the timeout, the next
     * discovery pages again and the red-Sync PIN still applies. */
    check_pair("Nintendo RVL-CNT-01", 0, 0, 1, 1, 0, 1);
    printf(failed ? "%d Wii pairing checks failed\n" : "all Wii legacy pairing checks passed\n", failed);
    return failed != 0;
}
