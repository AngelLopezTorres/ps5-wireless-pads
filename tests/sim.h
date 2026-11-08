/* A simulated Bluetooth controller with one game pad in range, behind the
 * hci_t interface. It answers HCI commands as a controller would, and
 * plays the pad's side of pairing, L2CAP, SDP and HID, so the host stack
 * can be exercised end to end without hardware. */
#ifndef PH_SIM_H
#define PH_SIM_H

#include "../src/hci.h"

#include <stdint.h>

typedef struct sim sim_t;

typedef enum {
    SIM_WAIT_PAIRING,   /* in pairing mode: waits for the host to find it */
    SIM_RECONNECT,      /* paired before: connects to the host by itself */
} sim_mode;

typedef struct {
    sim_mode mode;
    unsigned char addr[6];
    uint32_t cod;
    uint16_t vid, pid;
    /* The input report the pad streams once its interrupt channel is open
     * and it has seen an output report (or at once, if !needs_output). */
    unsigned char report[128];
    int report_len;
    int needs_output;
    /* Streamed before the first output report, if any (a pad's simple
     * mode before it is woken up). */
    unsigned char basic[64];
    int basic_len;
    /* The HID report descriptor its SDP record serves, in pieces. */
    unsigned char desc[512];
    int desc_len;
    /* Drop every Nth event and ACL packet, as the system's driver would
     * take them. 0: none. */
    int drop_every;
    unsigned seed;
    const char *remote_name;
    int no_name_reply;
    int name_status;
    int no_host_address;
    int legacy_pin; /* 1: Wii host-address PIN; 2: ASCII0000; 0: SSP. */
    /* The first N pages go unanswered: the controller pages until told to
     * cancel. While it pages or asks a name, Create Connection is refused
     * with "command disallowed", as a real controller does. */
    int unanswered_pages;
    /* A Wii Remote: answers status and extension-id requests, and streams
     * `report` once a reporting mode is set (`basic` before). 1: with a
     * Guitar Hero guitar plugged in; 2: with nothing plugged in. */
    int wii;
    /* L2CAP trouble seen on the PS5: the answer to the host's first HID
     * control-channel request is lost (to the system's driver); a repeat
     * of a request already taken, same cid, is ignored; HID channel
     * requests are answered "pending" first, success this many ms later. */
    int lose_first_hid_rsp;
    int ignore_repeat_cid;
    int pending_ms;
    /* The first N pages end in a page timeout (0x04) after 15 s. */
    int page_timeouts;
    /* The first event with this code goes to the system's driver, not the
     * host. 0: none. */
    int lose_event;
    /* Seen with a Wii Remote on the PS5: encryption takes this many ms to
     * start, and the chip holds the host's ACL data until it is on (no
     * completed-packet count); asked again meanwhile, it answers 0x23. */
    int enc_delay_ms;
    /* A Wii Remote holding a HID channel refuses a second request for it
     * with another cid ("no resources", 4). */
    int refuse_second_req;
    /* The first HID control-channel request is answered (success) only
     * this many ms later. */
    int late_rsp_ms;
    /* The chip's page scan as the PS5's system sets it: 208 ms every
     * 417 ms, interlaced. While it is that busy, pages time out (15 s). */
    int busy_page_scan;
    /* The chip sends the host's ACL packets one every tx_ms ms, with 8
     * buffers; a packet sent to a full chip is dropped with a Data
     * Buffer Overflow event. 0: at once. */
    int tx_ms;
    /* Seen on the PS5, the first reconnect after PadBridge starts: the
     * remote's own control-channel request is lost (never reaches the
     * host). It refuses the host's requests for it ("no resources") and
     * gives up on the link 10 s later, connecting again 1.6 s after. A
     * host's disconnect meanwhile is taken the same way (assumed: not seen
     * on the console). Only the first time. */
    int lose_own_req;
} sim_pad;

sim_t *sim_new(const sim_pad *pad);
void   sim_free(sim_t *s);
hci_t  sim_hci(sim_t *s);

/* Virtual clock, advanced by each pump. */
long   sim_now(void);
int    sim_diag_calls(void);   /* times the host asked the transport for its self-report */

void   sim_power_on(sim_t *s);      /* SIM_RECONNECT: page the host */
void   sim_disconnect(sim_t *s);    /* the pad switches off */
void   sim_set_report(sim_t *s, const unsigned char *r, int len);
void   sim_give_key(sim_t *s, const unsigned char *key);   /* paired before */
void   sim_go_deaf(sim_t *s);
/* Garbage from the air: frames and events no well-behaved pad sends. */
void   sim_inject_acl(sim_t *s, unsigned cid, const unsigned char *d, int n);
void   sim_inject_event(sim_t *s, unsigned char code, const unsigned char *p, int n);    /* stops answering, as after rest mode */

/* What the host did. */
int    sim_sdp_queries(const sim_t *s);
int    sim_outputs(const sim_t *s);
int    sim_sdp_continuations(const sim_t *s);
int    sim_last_output(const sim_t *s, unsigned char *out, int max);
int    sim_link_up(const sim_t *s);
int    sim_pin_replies(const sim_t *s, unsigned char out[23]);
int    sim_auth_successes(const sim_t *s);
int    sim_dropped(const sim_t *s);
int    sim_scan(const sim_t *s);           /* the controller's scan enable */
void   sim_set_scan(sim_t *s, int v);     /* someone else changes it */
void   sim_stale_link(sim_t *s, unsigned handle);   /* the chip still holds the pad's link from an earlier run */
int    sim_stale_cleared(const sim_t *s);
int    sim_refused(const sim_t *s);        /* Create Connections refused while busy */
int    sim_page_cancels(const sim_t *s);
int    sim_name_cancels(const sim_t *s);
int    sim_names(const sim_t *s);          /* Remote Name Requests */
int    sim_names_unlinked(const sim_t *s); /* ... sent with no link to the pad */
int    sim_pages(const sim_t *s);          /* Create Connections to the pad */
int    sim_enc_requests(const sim_t *s);   /* Set Connection Encryption commands */
int    sim_refused_chans(const sim_t *s);  /* HID channel requests refused as a second one */
int    sim_ctrl_reqs(const sim_t *s);      /* HID control-channel requests */
void   sim_page_scan(const sim_t *s, unsigned *interval, unsigned *window, unsigned *type);
int    sim_busy_pages(const sim_t *s);     /* pages sent while page scan was busy */
int    sim_inquiries(const sim_t *s);
int    sim_inquiry_cancels(const sim_t *s);
long   sim_last_inquiry_at(const sim_t *s);
void   sim_set_mode(sim_t *s, sim_mode mode);  /* paired now: reconnects by itself */
int    sim_overflows(const sim_t *s);      /* ACL packets dropped, chip full */
int    sim_host_disconnects(const sim_t *s);    /* Disconnect commands from the host */
void   sim_last_page(const sim_t *s, unsigned *mode, unsigned *offset);   /* its scan mode, clock offset */

#endif
