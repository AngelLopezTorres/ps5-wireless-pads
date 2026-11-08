#include "sim.h"
#include "../src/util.h"

#include <stdlib.h>
#include <string.h>

#define QLEN    256
#define PKT     1100
#define HANDLE  0x0041

/* The pad's ends of its channels. */
#define RCID_SDP  0x0070
#define RCID_CTRL 0x0071
#define RCID_INTR 0x0072

typedef struct {
    unsigned char d[QLEN][PKT];
    int len[QLEN];
    int head, count;
} queue;

typedef enum { RC_CLOSED, RC_CONNECTING, RC_CONFIG, RC_OPEN } rc_state;

typedef struct {
    unsigned psm, cid, peer;
    rc_state st;
    int cfg_in, cfg_out;
    long t_cfg;
    unsigned char pend_id;          /* a "pending" answer to complete... */
    long pend_until;                /* ... then, 0: none */
    int reqs;                       /* connection requests from the host */
} rchan;

struct sim {
    unsigned stale;                 /* handle+1 of a link left by an earlier run, 0: none */
    int stale_cleared;
    sim_pad pad;
    queue ev, acl;
    int connected, enc, has_key, got_output;
    unsigned char key[16];
    rchan sdp, ctrl, intr;
    unsigned char sig_id;
    int sdp_queries, sdp_conts, outputs, dropped;
    unsigned drop_count;
    unsigned char last_out[128];
    int last_out_len;
    long t_report;
    int deaf;
    int ssp, scan;
    int pin_replies, auth_successes;
    unsigned char last_pin[23];
    int name_busy, page_busy, pages;
    int refused, page_cancels, name_cancels;
    int names, names_unlinked;      /* name requests; those before a link */
    int page_timeouts;              /* pages ended with status 0x04 so far */
    long page_fail_at;              /* when the running page times out, 0: none */
    unsigned last_page_mode, last_page_offset;
    int lost_event, enc_requests, refused_chans;
    unsigned ps_int, ps_win, ps_type;   /* page scan activity and type */
    int busy_pages;
    int inquiries, inquiry_cancels;
    long last_inquiry_at;
    long enc_at;                    /* encryption comes on then, 0: not starting */
    queue txq;                      /* the host's ACL data the chip still holds (tx_ms) */
    long t_tx;                      /* when the chip last sent one */
    int overflows;
    int own_lost, own_lost_done;    /* lose_own_req: lost now; happened once */
    long t_own_lost, repage_at;
    int host_disconnects;
    queue held;                     /* the host's ACL data until then */
};

static long g_clock = 1000;

long sim_now(void) { return g_clock; }

/* ---- queues ----------------------------------------------------------------- */

static void q_put(queue *q, const unsigned char *p, int n)
{
    int slot;

    if (q->count == QLEN) return;
    slot = (q->head + q->count) % QLEN;
    memcpy(q->d[slot], p, (size_t)n);
    q->len[slot] = n;
    q->count++;
}

static int q_get(queue *q, unsigned char *out, int max)
{
    int n;

    if (!q->count) return 0;
    n = q->len[q->head] < max ? q->len[q->head] : max;
    memcpy(out, q->d[q->head], (size_t)n);
    q->head = (q->head + 1) % QLEN;
    q->count--;
    return n;
}

/* ---- what the controller reports ------------------------------------------ */

static void event(sim_t *s, unsigned char code, const unsigned char *p, int n)
{
    unsigned char e[260];

    if (s->pad.lose_event && code == s->pad.lose_event && !s->lost_event) {
        s->lost_event = 1;              /* taken by the system's driver */
        return;
    }
    e[0] = code;
    e[1] = (unsigned char)n;
    memcpy(e + 2, p, (size_t)n);
    q_put(&s->ev, e, 2 + n);
}

static void cmd_complete(sim_t *s, unsigned op, const unsigned char *ret, int n)
{
    unsigned char p[64];

    p[0] = 1;
    put16(p + 1, op);
    memcpy(p + 3, ret, (size_t)n);
    event(s, 0x0E, p, 3 + n);
}

static void cmd_ok(sim_t *s, unsigned op)
{
    static const unsigned char ok[1] = { 0 };
    cmd_complete(s, op, ok, 1);
}

static void cmd_status(sim_t *s, unsigned op, unsigned char status)
{
    unsigned char p[4];

    p[0] = status;
    p[1] = 1;
    put16(p + 2, op);
    event(s, 0x0F, p, 4);
}

static void ev_addr(sim_t *s, unsigned char code)
{
    event(s, code, s->pad.addr, 6);
}

static void ev_handle(sim_t *s, unsigned char code, unsigned char status, int extra, unsigned char x)
{
    unsigned char p[4];

    p[0] = status;
    put16(p + 1, HANDLE);
    p[3] = x;
    event(s, code, p, 3 + extra);
}

static void conn_complete(sim_t *s)
{
    unsigned char p[11];

    p[0] = 0;
    put16(p + 1, HANDLE);
    memcpy(p + 3, s->pad.addr, 6);
    p[9] = 1;
    p[10] = 0;
    event(s, 0x03, p, 11);
    s->connected = 1;
}

static void new_key(sim_t *s)
{
    unsigned char p[23];
    int i;

    for (i = 0; i < 16; i++) s->key[i] = (unsigned char)(0xA0 + i);
    s->has_key = 1;
    memcpy(p, s->pad.addr, 6);
    memcpy(p + 6, s->key, 16);
    p[22] = 0x04;
    event(s, 0x18, p, 23);
}

/* ---- the pad's L2CAP ---------------------------------------------------- */

static void acl_to_host(sim_t *s, unsigned cid, const unsigned char *d, int n)
{
    unsigned char pkt[PKT];

    put16(pkt, HANDLE | 0x2000);
    put16(pkt + 2, (unsigned)(4 + n));
    put16(pkt + 4, (unsigned)n);
    put16(pkt + 6, cid);
    memcpy(pkt + 8, d, (size_t)n);
    q_put(&s->acl, pkt, 8 + n);
}

static void sig(sim_t *s, unsigned char code, unsigned char id, const unsigned char *d, int n)
{
    unsigned char p[64];

    p[0] = code;
    p[1] = id;
    put16(p + 2, (unsigned)n);
    memcpy(p + 4, d, (size_t)n);
    acl_to_host(s, 1, p, 4 + n);
}

static rchan *rc_by_cid(sim_t *s, unsigned cid)
{
    if (cid == RCID_SDP) return &s->sdp;
    if (cid == RCID_CTRL) return &s->ctrl;
    if (cid == RCID_INTR) return &s->intr;
    return NULL;
}

static rchan *rc_by_psm(sim_t *s, unsigned psm)
{
    if (psm == 0x01) return &s->sdp;
    if (psm == 0x11) return &s->ctrl;
    if (psm == 0x13) return &s->intr;
    return NULL;
}

static void rc_send_cfg(sim_t *s, rchan *c)
{
    unsigned char r[4];

    put16(r, c->peer);
    put16(r + 2, 0);
    sig(s, 0x04, ++s->sig_id, r, 4);
    c->t_cfg = g_clock;
}

static void rc_check(rchan *c)
{
    if (c->st == RC_CONFIG && c->cfg_in && c->cfg_out) c->st = RC_OPEN;
}

static void rc_connect(sim_t *s, rchan *c)
{
    unsigned char r[4];

    c->st = RC_CONNECTING;
    c->cfg_in = c->cfg_out = 0;
    if (c == &s->ctrl && s->pad.lose_own_req && !s->own_lost_done) {
        s->own_lost = s->own_lost_done = 1;     /* sent, never arrives */
        s->t_own_lost = g_clock;
        c->peer = 0;
        ++s->sig_id;
        return;
    }
    put16(r, c->psm);
    put16(r + 2, c->cid);
    sig(s, 0x02, ++s->sig_id, r, 4);
}

static void on_host_signaling(sim_t *s, const unsigned char *d, int n)
{
    unsigned char code = d[0], id = d[1], r[8];
    const unsigned char *c = d + 4;
    rchan *ch;

    (void)n;
    switch (code) {
    case 0x02:  /* connection request: psm, scid */
        ch = rc_by_psm(s, le16(c));
        if (!ch) break;
        ch->reqs++;
        if ((s->pad.refuse_second_req || (s->own_lost && ch == &s->ctrl)) && ch != &s->sdp &&
            ch->st != RC_CLOSED && ch->peer != le16(c + 2)) {
            s->refused_chans++;
            put16(r, 0);
            put16(r + 2, le16(c + 2));
            put16(r + 4, 4);            /* no resources available */
            put16(r + 6, 0);
            sig(s, 0x03, id, r, 8);
            break;
        }
        /* A repeat of a request it already took, with the same cid. */
        if (s->pad.ignore_repeat_cid && ch->st != RC_CLOSED && ch->peer == le16(c + 2)) break;
        ch->peer = le16(c + 2);
        ch->st = RC_CONFIG;
        ch->cfg_in = ch->cfg_out = 0;
        ch->t_cfg = g_clock;
        if (ch == &s->ctrl && s->pad.lose_first_hid_rsp && ch->reqs == 1) break;  /* answer lost */
        if (ch == &s->ctrl && s->pad.late_rsp_ms && ch->reqs == 1) {   /* answered late */
            ch->pend_id = id;
            ch->pend_until = g_clock + s->pad.late_rsp_ms;
            break;
        }
        if (ch != &s->sdp && s->pad.pending_ms) {
            put16(r, 0);
            put16(r + 2, ch->peer);
            put16(r + 4, 1);            /* pending */
            put16(r + 6, 0);
            sig(s, 0x03, id, r, 8);
            ch->pend_id = id;
            ch->pend_until = g_clock + s->pad.pending_ms;
            break;
        }
        put16(r, ch->cid);
        put16(r + 2, ch->peer);
        put16(r + 4, 0);
        put16(r + 6, 0);
        sig(s, 0x03, id, r, 8);
        rc_send_cfg(s, ch);
        break;
    case 0x03:  /* connection response: dcid, scid, result */
        ch = rc_by_cid(s, le16(c + 2));
        if (!ch || le16(c + 4) != 0) break;
        ch->peer = le16(c);
        ch->st = RC_CONFIG;
        rc_send_cfg(s, ch);
        break;
    case 0x04:  /* configuration request: dcid (ours) */
        ch = rc_by_cid(s, le16(c));
        if (!ch) break;
        put16(r, ch->peer);
        put16(r + 2, 0);
        put16(r + 4, 0);
        sig(s, 0x05, id, r, 6);
        ch->cfg_in = 1;
        rc_check(ch);
        break;
    case 0x05:  /* configuration response: scid (ours) */
        ch = rc_by_cid(s, le16(c));
        if (!ch) break;
        ch->cfg_out = 1;
        rc_check(ch);
        break;
    case 0x06:  /* disconnection request: dcid (ours), scid */
        sig(s, 0x07, id, c, 4);
        ch = rc_by_cid(s, le16(c));
        if (ch) ch->st = RC_CLOSED;
        break;
    default:
        break;
    }
}

/* A data element sequence header around `len` bytes. */
static int seq(unsigned char *p, int len)
{
    if (len < 256) { p[0] = 0x35; p[1] = (unsigned char)len; return 2; }
    p[0] = 0x36; p[1] = (unsigned char)(len >> 8); p[2] = (unsigned char)len;
    return 3;
}

/* The attribute list the pad answers with, for the service asked. */
static int sdp_list(sim_t *s, unsigned uuid, unsigned char *out)
{
    unsigned char a[700];
    int n = 0, l = 0;

    if (uuid == 0x1200) {       /* {0x0201 vid, 0x0202 pid, 0x0203 version} */
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x01;
        a[n++] = 0x09; a[n++] = (unsigned char)(s->pad.vid >> 8); a[n++] = (unsigned char)s->pad.vid;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x02;
        a[n++] = 0x09; a[n++] = (unsigned char)(s->pad.pid >> 8); a[n++] = (unsigned char)s->pad.pid;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x03;
        a[n++] = 0x09; a[n++] = 0x01; a[n++] = 0x00;
    } else if (uuid == 0x1124 && s->pad.desc_len) {
        /* {0x0206, {{0x22, descriptor}}} */
        unsigned char inner[600];
        int i = 0, dl = s->pad.desc_len;
        inner[i++] = 0x08; inner[i++] = 0x22;
        if (dl < 256) { inner[i++] = 0x25; inner[i++] = (unsigned char)dl; }
        else { inner[i++] = 0x26; inner[i++] = (unsigned char)(dl >> 8); inner[i++] = (unsigned char)dl; }
        memcpy(inner + i, s->pad.desc, (size_t)dl);
        i += dl;
        a[n++] = 0x09; a[n++] = 0x02; a[n++] = 0x06;
        {
            unsigned char mid[620];
            int m = seq(mid, i);
            memcpy(mid + m, inner, (size_t)i);
            m += i;
            n += seq(a + n, m);
            memcpy(a + n, mid, (size_t)m);
            n += m;
        }
    } else {
        return seq(out, 0);
    }
    /* one record's attribute list, inside the list of records */
    {
        unsigned char rec[720];
        int r = seq(rec, n);
        memcpy(rec + r, a, (size_t)n);
        r += n;
        l = seq(out, r);
        memcpy(out + l, rec, (size_t)r);
        l += r;
    }
    return l;
}

#define SDP_CHUNK 48

static void on_host_sdp(sim_t *s, const unsigned char *d, int n)
{
    unsigned char list[800], rsp[128];
    int len, p, cl, off = 0, chunk;
    unsigned uuid;

    if (n < 12 || d[0] != 0x06) return;
    uuid = (unsigned)d[8] << 8 | d[9];          /* pattern {UUID16} */
    p = 5 + 2 + d[6];                           /* past the pattern */
    p += 2;                                     /* max byte count */
    p += 2 + d[p + 1];                          /* attribute id list */
    if (p >= n) return;
    cl = d[p];
    if (cl == 2) {
        off = d[p + 1] << 8 | d[p + 2];
        s->sdp_conts++;
    } else {
        s->sdp_queries++;
    }

    len = sdp_list(s, uuid, list);
    chunk = len - off < SDP_CHUNK ? len - off : SDP_CHUNK;
    rsp[0] = 0x07;
    rsp[1] = d[1];
    rsp[2] = d[2];
    rsp[5] = (unsigned char)(chunk >> 8);
    rsp[6] = (unsigned char)chunk;
    memcpy(rsp + 7, list + off, (size_t)chunk);
    if (off + chunk < len) {
        rsp[7 + chunk] = 2;
        rsp[8 + chunk] = (unsigned char)((off + chunk) >> 8);
        rsp[9 + chunk] = (unsigned char)(off + chunk);
        n = 10 + chunk;
    } else {
        rsp[7 + chunk] = 0;
        n = 8 + chunk;
    }
    rsp[3] = (unsigned char)((n - 5) >> 8);
    rsp[4] = (unsigned char)(n - 5);
    acl_to_host(s, s->sdp.peer, rsp, n);
}

/* ---- hci_ops ------------------------------------------------------------- */

static int op_cmd(void *ctx, unsigned op, const void *params, int plen)
{
    sim_t *s = ctx;
    const unsigned char *p = params;

    (void)plen;
    if (s->deaf) return 1;
    switch (op) {
    case 0x0C55: {      /* Read Simple Pairing Mode */
        unsigned char r[2] = { 0, (unsigned char)s->ssp };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x0C56:
        s->ssp = p[0];
        cmd_ok(s, op);
        break;
    case 0x0C19: {      /* Read Scan Enable */
        unsigned char r[2] = { 0, (unsigned char)s->scan };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x0C1A:
        s->scan = p[0];
        cmd_ok(s, op);
        break;
    case 0x0C17: {      /* Read Page Timeout: 0x2000 slots, the default */
        unsigned char r[3] = { 0, 0x00, 0x20 };
        cmd_complete(s, op, r, 3);
        break;
    }
    case 0x0C1B: {      /* Read Page Scan Activity */
        unsigned char r[5] = { 0 };
        put16(r + 1, s->ps_int);
        put16(r + 3, s->ps_win);
        cmd_complete(s, op, r, 5);
        break;
    }
    case 0x0402:        /* Inquiry Cancel */
        s->inquiry_cancels++;
        cmd_ok(s, op);
        break;
    case 0x0C1C:        /* Write Page Scan Activity */
        s->ps_int = le16(p);
        s->ps_win = le16(p + 2);
        cmd_ok(s, op);
        break;
    case 0x0C47:        /* Write Page Scan Type */
        s->ps_type = p[0];
        cmd_ok(s, op);
        break;
    case 0x0C1D: {      /* Read Inquiry Scan Activity: 1.28 s, 11.25 ms */
        unsigned char r[5] = { 0, 0x00, 0x08, 0x12, 0x00 };
        cmd_complete(s, op, r, 5);
        break;
    }
    case 0x0C44: {      /* Read Inquiry Mode: extended */
        unsigned char r[2] = { 0, 2 };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x0C46: {      /* Read Page Scan Type */
        unsigned char r[2] = { 0, (unsigned char)s->ps_type };
        cmd_complete(s, op, r, 2);
        break;
    }
    case 0x1005: {      /* Read Buffer Size */
        unsigned char r[8] = { 0 };
        put16(r + 1, 1021);
        r[3] = 64;
        put16(r + 4, 8);
        cmd_complete(s, op, r, 8);
        break;
    }
    case 0x1009: {
        /* Includes a binary zero so PIN tests cannot accidentally use strlen. */
        unsigned char r[7] = {0, 0x66, 0, 0x44, 0x33, 0x22, 0x11};
        if (s->pad.no_host_address) memset(r + 1, 0, 6);
        cmd_complete(s, op, r, sizeof r);
        break;
    }
    case 0x0419: {
        unsigned char r[255] = {0};
        const char *name = s->pad.remote_name ? s->pad.remote_name : "Simulated Controller";
        cmd_status(s, op, 0);
        s->names++;
        if (!s->connected) s->names_unlinked++;
        if (s->pad.no_name_reply) { s->name_busy = 1; break; }
        r[0] = (unsigned char)s->pad.name_status;
        memcpy(r + 1, s->pad.addr, 6);
        strncpy((char *)r + 7, name, sizeof r - 8);
        event(s, 0x07, r, sizeof r);
        break;
    }
    case 0x0401:        /* Inquiry */
        cmd_status(s, op, 0);
        s->inquiries++;
        s->last_inquiry_at = g_clock;
        if (s->pad.mode == SIM_WAIT_PAIRING && !s->connected) {
            unsigned char r[255] = { 0 };
            r[0] = 1;
            memcpy(r + 1, s->pad.addr, 6);
            r[7] = 1;
            r[9] = (unsigned char)s->pad.cod;
            r[10] = (unsigned char)(s->pad.cod >> 8);
            r[11] = (unsigned char)(s->pad.cod >> 16);
            put16(r + 12, 0x1234);
            event(s, 0x2F, r, 255);
        }
        break;
    case 0x0405:        /* Create Connection */
        if (s->stale) {                 /* an earlier run's link still holds the pad */
            cmd_status(s, op, 0x0B);    /* connection already exists */
            break;
        }
        if (s->name_busy || s->page_busy) {
            s->refused++;
            cmd_status(s, op, 0x0C);    /* command disallowed */
            break;
        }
        cmd_status(s, op, 0);
        if (memcmp(p, s->pad.addr, 6) != 0) break;
        s->last_page_mode = p[8];
        s->last_page_offset = (unsigned)le16(p + 10);
        if (s->pages++ < s->pad.unanswered_pages) { s->page_busy = 1; break; }
        if (s->ps_type == 1 || s->ps_win * 4 > s->ps_int) {     /* the radio is scanning half the time */
            s->busy_pages++;
            s->page_busy = 1;
            s->page_fail_at = g_clock + 15000;
            break;
        }
        if (s->page_timeouts < s->pad.page_timeouts) {
            s->page_timeouts++;
            s->page_busy = 1;
            s->page_fail_at = g_clock + 15000;  /* the controller's page timeout */
            break;
        }
        conn_complete(s);
        break;
    case 0x1405: {      /* Read RSSI */
        unsigned char r[4] = { 0, p[0], p[1], (unsigned char)-48 };
        cmd_complete(s, op, r, 4);
        break;
    }
    case 0x0408: {      /* Create Connection Cancel */
        unsigned char r[7] = { 0 }, c[11] = { 0 };
        memcpy(r + 1, p, 6);
        if (!s->page_busy) {
            r[0] = 0x0B;                /* nothing to cancel */
            cmd_complete(s, op, r, 7);
            break;
        }
        s->page_busy = 0;
        s->page_fail_at = 0;
        s->page_cancels++;
        cmd_complete(s, op, r, 7);
        c[0] = 0x02;                    /* the page's end, as cancelled */
        memcpy(c + 3, p, 6);
        c[9] = 1;
        event(s, 0x03, c, 11);
        break;
    }
    case 0x041A: {      /* Remote Name Request Cancel */
        unsigned char r[7] = { 0 }, n[255] = { 0 };
        memcpy(r + 1, p, 6);
        if (!s->name_busy) {
            r[0] = 0x02;
            cmd_complete(s, op, r, 7);
            break;
        }
        s->name_busy = 0;
        s->name_cancels++;
        cmd_complete(s, op, r, 7);
        n[0] = 0x02;
        memcpy(n + 1, p, 6);
        event(s, 0x07, n, sizeof n);
        break;
    }
    case 0x0409:        /* Accept Connection Request */
        cmd_status(s, op, 0);
        conn_complete(s);
        /* A pad reconnecting by itself authenticates itself. */
        ev_addr(s, 0x17);
        break;
    case 0x0411:        /* Authentication Requested */
        cmd_status(s, op, 0);
        ev_addr(s, 0x17);
        break;
    case 0x040C:        /* Link Key Negative Reply: pair */
        cmd_ok(s, op);
        ev_addr(s, s->pad.legacy_pin ? 0x16 : 0x31);
        break;
    case 0x040D: {
        static const unsigned char wii_pin[6] = {0x66, 0, 0x44, 0x33, 0x22, 0x11};
        int ok = plen == 23 && !memcmp(p, s->pad.addr, 6);
        s->pin_replies++;
        if (plen == 23) memcpy(s->last_pin, p, 23);
        if (s->pad.legacy_pin == 1) ok = ok && p[6] == 6 && !memcmp(p + 7, wii_pin, 6);
        else ok = ok && p[6] == 4 && !memcmp(p + 7, "0000", 4);
        {
            unsigned char r[7] = { 0 };     /* status, address */
            if (plen >= 6) memcpy(r + 1, p, 6);
            cmd_complete(s, op, r, 7);
        }
        if (ok) { new_key(s); s->auth_successes++; }
        ev_handle(s, 0x06, ok ? 0 : 5, 0, 0);
        break;
    }
    case 0x042B: {      /* IO Capability Reply */
        unsigned char r[10] = { 0 };
        cmd_ok(s, op);
        memcpy(r, s->pad.addr, 6);
        event(s, 0x33, r, 10);
        break;
    }
    case 0x042C:        /* User Confirmation Reply */
        cmd_ok(s, op);
        s->auth_successes++;
        new_key(s);
        ev_handle(s, 0x06, 0, 0, 0);
        break;
    case 0x040B:        /* Link Key Reply */
        cmd_ok(s, op);
        if (s->has_key && memcmp(p + 6, s->key, 16) == 0) {
            if (s->pad.mode == SIM_RECONNECT) {
                s->enc = 1;                     /* the pad encrypts by itself */
                ev_handle(s, 0x08, 0, 1, 1);
            } else {
                ev_handle(s, 0x06, 0, 0, 0);
            }
        } else {
            ev_handle(s, 0x06, 0x05, 0, 0);     /* authentication failure */
        }
        break;
    case 0x0413:        /* Set Connection Encryption */
        s->enc_requests++;
        if (s->pad.enc_delay_ms && !s->enc) {
            if (s->enc_at) {
                cmd_status(s, op, 0x23);    /* LMP error transaction collision */
                break;
            }
            cmd_status(s, op, 0);
            s->enc_at = g_clock + s->pad.enc_delay_ms;
            break;
        }
        cmd_status(s, op, 0);
        s->enc = 1;
        ev_handle(s, 0x08, 0, 1, 1);
        break;
    case 0x0406:        /* Disconnect */
        if (s->stale) {
            if (le16(p) == s->stale - 1) {
                s->stale = 0;
                s->stale_cleared++;
                cmd_status(s, op, 0);
            } else {
                cmd_status(s, op, 0x02);    /* unknown connection handle */
            }
            break;
        }
        cmd_status(s, op, 0);
        s->host_disconnects++;
        if (s->own_lost) {
            sim_disconnect(s);
            s->repage_at = g_clock + 1600;
            break;
        }
        sim_disconnect(s);
        break;
    default:
        cmd_ok(s, op);
        break;
    }
    return 1;
}

/* A Wii Remote's answers: status 0x20 to 0x15, the extension id (0x21) to
 * a read of 0xA400FA, and the full report once a reporting mode is set. */
static void wii_answer(sim_t *s, const unsigned char *o, int n)
{
    static const unsigned char guitar_id[6] = { 0x00, 0x00, 0xA4, 0x20, 0x01, 0x03 };
    unsigned char f[23];
    int ext = s->pad.wii == 1;

    memset(f, 0, sizeof f);
    f[0] = 0xA1;
    if (o[0] == 0x15) {
        f[1] = 0x20;
        f[4] = ext ? 0x12 : 0x10;       /* LED 1; extension plugged in */
        f[7] = 0xC0;                    /* battery */
        acl_to_host(s, s->intr.peer, f, 8);
    } else if (o[0] == 0x17 && n >= 7 && o[2] == 0xA4 && o[4] == 0xFA) {
        f[1] = 0x21;
        f[4] = ext ? 0x50 : 0x57;       /* 6 bytes; or error 7: nothing there */
        f[5] = 0x00; f[6] = 0xFA;
        if (ext) memcpy(f + 7, guitar_id, 6);
        acl_to_host(s, s->intr.peer, f, 23);
    } else if (o[0] == 0x12 && n >= 3) {
        s->got_output = 1;
    }
}

static int deliver(sim_t *s, const unsigned char *pkt, int len);

static int op_acl_send(void *ctx, const unsigned char *pkt, int len)
{
    sim_t *s = ctx;

    if (len < 8 || !s->connected) return 1;
    if (s->pad.tx_ms) {                 /* the chip's buffers */
        if (s->txq.count + s->held.count >= 8) {
            unsigned char t = 0x01;     /* ACL */
            s->overflows++;
            event(s, 0x1A, &t, 1);
            return 1;
        }
        if (!s->txq.count && !s->held.count) s->t_tx = g_clock;
    }
    if (s->enc_at && !s->enc) {         /* held, not reported sent */
        q_put(&s->held, pkt, len);
        return 1;
    }
    if (s->pad.tx_ms) {
        q_put(&s->txq, pkt, len);
        return 1;
    }
    return deliver(s, pkt, len);
}

static int deliver(sim_t *s, const unsigned char *pkt, int len)
{
    unsigned cid;
    const unsigned char *d;
    int n;
    unsigned char ncp[5];

    if (len < 8 || !s->connected) return 1;
    n = (int)le16(pkt + 4);
    cid = le16(pkt + 6);
    d = pkt + 8;

    ncp[0] = 1;
    put16(ncp + 1, HANDLE);
    put16(ncp + 3, 1);
    event(s, 0x13, ncp, 5);

    if (cid == 1) on_host_signaling(s, d, n);
    else if (cid == RCID_SDP && s->sdp.st == RC_OPEN) on_host_sdp(s, d, n);
    else if (cid == RCID_INTR && n >= 2 && d[0] == 0xA2) {
        s->outputs++;
        if (s->pad.wii) wii_answer(s, d + 1, n - 1);
        else s->got_output = 1;
        s->last_out_len = n < (int)sizeof s->last_out ? n : (int)sizeof s->last_out;
        memcpy(s->last_out, d, (size_t)s->last_out_len);
    }
    return 1;
}

/* The pad's own behaviour between packets. */
static void pad_step(sim_t *s)
{
    rchan *all[3];
    int i;

    if (s->repage_at && g_clock >= s->repage_at && !s->connected) {
        s->repage_at = 0;
        sim_power_on(s);
    }
    if (s->own_lost && s->connected && g_clock - s->t_own_lost >= 10000) {
        sim_disconnect(s);                  /* gives up, and tries again */
        s->repage_at = g_clock + 1600;
    }
    if (!s->connected || !s->enc) return;

    /* L2CAP's retransmission timer: an unanswered request is sent again. */
    all[0] = &s->sdp; all[1] = &s->ctrl; all[2] = &s->intr;
    for (i = 0; i < 3; i++) {
        rchan *ch = all[i];
        if (ch->pend_until && g_clock >= ch->pend_until) {
            unsigned char r[8];
            ch->pend_until = 0;
            put16(r, ch->cid);
            put16(r + 2, ch->peer);
            put16(r + 4, 0);
            put16(r + 6, 0);
            sig(s, 0x03, ch->pend_id, r, 8);
            rc_send_cfg(s, ch);
        } else if (!ch->pend_until && ch->st == RC_CONFIG && !ch->cfg_out && g_clock - ch->t_cfg >= 1000) {
            rc_send_cfg(s, ch);
        }
    }

    if (s->pad.mode == SIM_RECONNECT) {
        if (s->ctrl.st == RC_CLOSED) rc_connect(s, &s->ctrl);
        else if (s->ctrl.st == RC_OPEN && s->intr.st == RC_CLOSED) rc_connect(s, &s->intr);
    }
    if (s->intr.st == RC_OPEN && g_clock - s->t_report >= 8) {
        unsigned char f[130];
        int full = s->got_output || !s->pad.needs_output;
        const unsigned char *r = full ? s->pad.report : s->pad.basic;
        int n = full ? s->pad.report_len : s->pad.basic_len;
        if (n > 0) {
            f[0] = 0xA1;
            memcpy(f + 1, r, (size_t)n);
            acl_to_host(s, s->intr.peer, f, 1 + n);
        }
        s->t_report = g_clock;
    }
}

static int op_pump(void *ctx, int timeout_ms)
{
    sim_t *s = ctx;

    g_clock += timeout_ms > 0 ? timeout_ms : 1;
    if (s->deaf) return 0;
    if (s->page_fail_at && g_clock >= s->page_fail_at) {
        unsigned char c[11] = { 0x04 };     /* page timeout */
        s->page_fail_at = 0;
        s->page_busy = 0;
        memcpy(c + 3, s->pad.addr, 6);
        c[9] = 1;
        event(s, 0x03, c, 11);
    }
    if (s->enc_at && g_clock >= s->enc_at) {
        unsigned char b[PKT];
        int n;
        s->enc_at = 0;
        s->enc = 1;
        ev_handle(s, 0x08, 0, 1, 1);
        while ((n = q_get(&s->held, b, (int)sizeof b)) > 0) {
            if (s->pad.tx_ms) q_put(&s->txq, b, n);
            else deliver(s, b, n);
        }
        s->t_tx = g_clock;
    }
    while (s->pad.tx_ms && s->txq.count && g_clock - s->t_tx >= s->pad.tx_ms) {
        unsigned char b[PKT];
        int n = q_get(&s->txq, b, (int)sizeof b);
        s->t_tx += s->pad.tx_ms;
        deliver(s, b, n);
    }
    pad_step(s);
    return s->ev.count || s->acl.count;
}

/* Drops every Nth ACL packet and completion report on the way to the host,
 * as sharing the controller with the system's driver does. */
static int lose(sim_t *s)
{
    /* Pseudo-random with a fixed seed: a fixed period could keep hitting
     * the same packet of a retry that sends exactly that many. */
    s->drop_count = s->drop_count * 1103515245u + 12345u;
    if (!s->pad.drop_every || (s->drop_count >> 16) % (unsigned)s->pad.drop_every) return 0;
    s->dropped++;
    return 1;
}

static int op_next_event(void *ctx, unsigned char *out, int max)
{
    sim_t *s = ctx;
    int n;

    while ((n = q_get(&s->ev, out, max)) > 0)
        if (out[0] != 0x13 || !lose(s)) return n;
    return 0;
}

static int op_next_acl(void *ctx, unsigned char *out, int max)
{
    sim_t *s = ctx;
    int n;

    while ((n = q_get(&s->acl, out, max)) > 0)
        if (!lose(s)) return n;
    return 0;
}

static void op_close(void *ctx) { (void)ctx; }

static int g_diag_calls;
static void op_diag(void *ctx) { (void)ctx; g_diag_calls++; }
int sim_diag_calls(void) { return g_diag_calls; }

static const hci_ops k_sim_ops = {
    op_cmd, op_acl_send, op_pump, op_next_event, op_next_acl, op_close, op_diag,
};

/* ---- public ---------------------------------------------------------------- */

sim_t *sim_new(const sim_pad *pad)
{
    sim_t *s = calloc(1, sizeof *s);

    s->pad = *pad;
    s->drop_count = pad->seed;
    s->sdp = (rchan){ .psm = 0x01, .cid = RCID_SDP, .st = RC_CLOSED };
    s->ctrl = (rchan){ .psm = 0x11, .cid = RCID_CTRL, .st = RC_CLOSED };
    s->intr = (rchan){ .psm = 0x13, .cid = RCID_INTR, .st = RC_CLOSED };
    s->ps_int = pad->busy_page_scan ? 0x029B : 0x0800;
    s->ps_win = pad->busy_page_scan ? 0x014D : 0x0012;
    s->ps_type = pad->busy_page_scan ? 1 : 0;
    ph_clock_hook = sim_now;
    return s;
}

void sim_free(sim_t *s) { free(s); }

hci_t sim_hci(sim_t *s)
{
    hci_t h = { &k_sim_ops, s };
    return h;
}

void sim_power_on(sim_t *s)
{
    unsigned char p[10];

    memcpy(p, s->pad.addr, 6);
    p[6] = (unsigned char)s->pad.cod;
    p[7] = (unsigned char)(s->pad.cod >> 8);
    p[8] = (unsigned char)(s->pad.cod >> 16);
    p[9] = 1;
    event(s, 0x04, p, 10);
}

void sim_disconnect(sim_t *s)
{
    unsigned char p[4];

    if (!s->connected) return;
    p[0] = 0;
    put16(p + 1, HANDLE);
    p[3] = 0x13;
    event(s, 0x05, p, 4);
    s->connected = s->enc = s->got_output = 0;
    s->enc_at = 0;
    s->held.count = 0;
    s->txq.count = 0;
    s->own_lost = 0;
    s->sdp.st = s->ctrl.st = s->intr.st = RC_CLOSED;
}

void sim_set_report(sim_t *s, const unsigned char *r, int len)
{
    memcpy(s->pad.report, r, (size_t)len);
    s->pad.report_len = len;
}

void sim_inject_acl(sim_t *s, unsigned cid, const unsigned char *d, int n) { acl_to_host(s, cid, d, n); }
void sim_inject_event(sim_t *s, unsigned char code, const unsigned char *p, int n) { event(s, code, p, n); }

void sim_go_deaf(sim_t *s)
{
    s->deaf = 1;
    s->ev.count = s->acl.count = 0;
}

void sim_give_key(sim_t *s, const unsigned char *key)
{
    memcpy(s->key, key, 16);
    s->has_key = 1;
}

int sim_sdp_queries(const sim_t *s) { return s->sdp_queries; }
int sim_outputs(const sim_t *s) { return s->outputs; }
int sim_sdp_continuations(const sim_t *s) { return s->sdp_conts; }
int sim_link_up(const sim_t *s) { return s->connected; }
int sim_dropped(const sim_t *s) { return s->dropped; }
int sim_scan(const sim_t *s) { return s->scan; }
void sim_stale_link(sim_t *s, unsigned handle) { s->stale = handle + 1; }
int sim_stale_cleared(const sim_t *s) { return s->stale_cleared; }
void sim_set_scan(sim_t *s, int v) { s->scan = v; }

int sim_last_output(const sim_t *s, unsigned char *out, int max)
{
    int n = s->last_out_len < max ? s->last_out_len : max;
    memcpy(out, s->last_out, (size_t)n);
    return n;
}

int sim_pin_replies(const sim_t *s, unsigned char out[23])
{
    if (out) memcpy(out, s->last_pin, 23);
    return s->pin_replies;
}
int sim_auth_successes(const sim_t *s) { return s->auth_successes; }
int sim_refused(const sim_t *s) { return s->refused; }
int sim_page_cancels(const sim_t *s) { return s->page_cancels; }
int sim_name_cancels(const sim_t *s) { return s->name_cancels; }
int sim_names(const sim_t *s) { return s->names; }
int sim_names_unlinked(const sim_t *s) { return s->names_unlinked; }
int sim_pages(const sim_t *s) { return s->pages; }
int sim_enc_requests(const sim_t *s) { return s->enc_requests; }
int sim_refused_chans(const sim_t *s) { return s->refused_chans; }
int sim_ctrl_reqs(const sim_t *s) { return s->ctrl.reqs; }
void sim_page_scan(const sim_t *s, unsigned *interval, unsigned *window, unsigned *type)
{
    *interval = s->ps_int; *window = s->ps_win; *type = s->ps_type;
}
int sim_busy_pages(const sim_t *s) { return s->busy_pages; }
int sim_inquiries(const sim_t *s) { return s->inquiries; }
int sim_inquiry_cancels(const sim_t *s) { return s->inquiry_cancels; }
long sim_last_inquiry_at(const sim_t *s) { return s->last_inquiry_at; }
void sim_set_mode(sim_t *s, sim_mode mode) { s->pad.mode = mode; }
int sim_overflows(const sim_t *s) { return s->overflows; }
int sim_host_disconnects(const sim_t *s) { return s->host_disconnects; }
void sim_last_page(const sim_t *s, unsigned *mode, unsigned *offset)
{
    *mode = s->last_page_mode;
    *offset = s->last_page_offset;
}
