/* PadBridge PS5 -- Bluetooth game controllers on the PlayStation 5.
 *
 * Pads from other consoles and brands connect to the console's own
 * Bluetooth chip, beside the DualSense, and appear to games as DualSense
 * controllers of the main user.
 *
 *   hci_usb.c   the Bluetooth chip, shared with the system's driver
 *   host.c      pairing, reconnection, L2CAP, SDP and HID (Classic)
 *   le.c        the same over Bluetooth LE
 *   profiles.c  each controller's reports
 *   ps5_vpad.c  the virtual DualSense each pad drives
 *   web.c       the menu: a page on port 8095
 *   (no console hotkey: a plain payload cannot read the DualSense; the menu opens from the media row)
 *
 * Stability comes first. It only touches what it created: its own Bluetooth
 * links, its own virtual pads, its own files under /data/padbridge. When
 * something it depends on fails, it says so (log, notification, the menu)
 * and stays out of the way instead of retrying forever.
 *
 * The menu is started first, before anything that can fail, so that it is
 * there to explain a failure. It is a web page: from a phone or a PC at the
 * address the start-up notification gives, or on the console itself by
 * holding a button combination on the DualSense (L2 + R2 + OPTIONS for 1.5
 * seconds unless /data/padbridge/config.ini says otherwise).
 *
 * Stopping: the menu's button, or create /data/padbridge/stop. It disconnects
 * its pads, removes its virtual pads, puts back the controller setting it
 * changed, and exits. It does the same by itself when the console starts
 * going to rest mode (read from SceSystemStateMgrInfo), so nothing of it is left half-way across a sleep.
 * Pairing: the menu's button, or create /data/padbridge/pair.
 */
#include "hci_usb.h"
#include "i18n.h"
#include "host.h"
#include "lock.h"
#include "log.h"
#include "netinfo.h"
#include "ps5_power.h"
#include "ps5_ui.h"
#include "ps5_apps.h"
#include "ps5_sysinfo.h"
#include "profiles.h"
#include "ps5_vpad.h"
#include "util.h"
#include "version.h"
#include "web.h"

#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STATE_DIR    "/data/padbridge"
#define LOG_PATH     STATE_DIR "/padbridge.log"
#define DB_PATH      STATE_DIR "/pads.db"
#define LOCK_PATH    STATE_DIR "/padbridge.lock"
#define CONFIG_PATH  STATE_DIR "/config.ini"
#define PAIR_FLAG    STATE_DIR "/pair"
#define STOP_FLAG    STATE_DIR "/stop"
#define NO_ICON_FLAG STATE_DIR "/no_icon"          /* do not put PadBridge in the media row */
#define LANG_PATH    STATE_DIR "/language"          /* the web page's language, also for notifications */
#define RM_ICON_FLAG STATE_DIR "/remove_icon"      /* take it out, once */
#define PAIR_SECONDS 60

#define OPEN_TRIES      5       /* opening Bluetooth: 5 tries, 3 s apart */
#define RAISED_TRIES    2       /* ... the first two with raised credentials */

/* Findable in the binary: strings PadBridge-PS5-*.elf | grep padbridge-version */
static const char g_version_tag[] __attribute__((used)) = "padbridge-version " PADBRIDGE_VERSION;
#define PADBRIDGE_AUTHOR "X-F1REBALL-X"
static const char g_author_tag[] __attribute__((used)) = "padbridge-author " PADBRIDGE_AUTHOR;

static volatile sig_atomic_t g_stop;
static volatile int g_web_stop;         /* the menu's stop button */
static volatile int g_retry;            /* the menu's retry button */
static volatile int g_bt_state = WEB_BT_TRYING;
static char g_bt_reason[200];
static const char *g_bt_reason_key = "";       /* the same, as a key the page translates */

static host_t *g_host;                  /* NULL until Bluetooth is up */
static web_t *g_web;
static int g_vpad_ok;                   /* virtual pads are possible */
static int g_bt_attempt;                /* Bluetooth tries made in this round */
static long g_bt_next;                  /* when the next one is due */
static int g_bt_plain_creds;            /* the chip answered only without raised credentials */

static void on_signal(int sig)
{
    (void)sig;
    g_stop = 1;
}

/* ---- virtual pads follow the controllers ---------------------------------- */

/* Each slot's model name, kept for its disconnection notice. */
static char g_slot_name[HOST_MAX_PADS][48];

static const char *slot_name(int slot)
{
    return slot >= 0 && slot < HOST_MAX_PADS && g_slot_name[slot][0] ? g_slot_name[slot] : tr(MSG_CONTROLLER);
}

static void on_connect(void *ud, int slot, const pad_info *info)
{
    (void)ud;
    if (slot >= 0 && slot < HOST_MAX_PADS)
        snprintf(g_slot_name[slot], sizeof g_slot_name[slot], "%s",
                 pad_display_name(info->vid, info->pid, info->profile));
    if (vpad_add(slot)) notify(tr(MSG_CONNECTED), slot_name(slot), slot + 1);
    else notify(tr(MSG_CONNECTED_NOVPAD), slot_name(slot), slot + 1);
}

static void on_state(void *ud, int slot, const pad_state *st)
{
    (void)ud;
    vpad_update(slot, st);
}

/* The menu's PS button: the same as holding PS on that pad. */
#define PS_HOLD_MS 700
static int web_press_ps(int slot)
{
    return g_vpad_ok && vpad_press_ps(slot, PS_HOLD_MS);
}

static void on_disconnect(void *ud, int slot)
{
    (void)ud;
    vpad_remove(slot);
    notify(tr(MSG_DISCONNECTED), slot_name(slot), slot + 1);
}

/* ---- Bluetooth, one try at a time ------------------------------------------ */

/* The host calls this whenever it waits for the chip, so the menu answers
 * even while a Bluetooth try is in progress (it used to freeze for ~5 s each
 * time, and the browser's pile-up of connections got "busy"). */
static void on_idle(void *ud)
{
    (void)ud;
    web_poll(g_web);
}

static void bt_begin(void)
{
    g_bt_attempt = 0;
    g_bt_next = 0;
    g_bt_reason[0] = '\0';
    g_bt_reason_key = "";
    g_bt_state = WEB_BT_TRYING;
    if (g_vpad_ok) vpad_creds_raise();
}

/* One try. The controller may answer only to a process that does not carry
 * raised credentials (not known), so the first
 * tries are made as they are and the rest with the original ones. Which
 * worked is logged. Virtual pads raise them again when they need to. */
static void bt_try(long now)
{
    host_events ev = { on_connect, on_state, on_disconnect, NULL, on_idle };
    hci_t hci;
    int attempt = ++g_bt_attempt;

    if (attempt == RAISED_TRIES + 1 && g_vpad_ok) {
        log_line("Bluetooth silent with raised credentials; trying with the original ones");
        if (vpad_creds_restore()) g_bt_plain_creds = 1;
    }
    if (hci_usb_open(&hci)) {
        g_host = host_open(hci, DB_PATH, &ev);
        if (g_host) {
            g_bt_state = WEB_BT_OK;
            log_line("Bluetooth ready after %d tr%s%s", attempt, attempt == 1 ? "y" : "ies",
                     g_bt_plain_creds ? ", answering only with the original credentials" : "");
            notify(tr(MSG_BT_READY), host_paired_count(g_host));
            return;
        }
        hci.ops->close(hci.ctx);
    }
    log_line("Bluetooth not ready (try %d of %d)", attempt, OPEN_TRIES);
    if (attempt >= OPEN_TRIES) {
        g_bt_state = WEB_BT_FAILED;
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "The Bluetooth chip does not answer (%d tries, with and without raised credentials). "
                 "Details in the log, 'diag' lines.", attempt);
        g_bt_reason_key = "nochip";
        log_line("Bluetooth unavailable: %s", g_bt_reason);
        notify("%s", tr(MSG_BT_NO_ANSWER));
        return;
    }
    g_bt_next = now + 3000;
}

static void bt_lost(const char *why)
{
    log_line("Bluetooth lost: %s", why);
    host_close(g_host);                 /* drops every pad and its virtual pad */
    g_host = NULL;
    g_bt_state = WEB_BT_FAILED;
    snprintf(g_bt_reason, sizeof g_bt_reason, "Bluetooth lost (%s). Press Retry.", why);
    g_bt_reason_key = "lost";
    notify("%s", tr(MSG_BT_LOST));
}

/* ---- flags ---------------------------------------------------------------- */

static void check_flags(long now)
{
    static long last_second;

    if (now / 1000 == last_second) return;
    last_second = now / 1000;
    if (access(STOP_FLAG, F_OK) == 0) {
        unlink(STOP_FLAG);
        g_stop = 1;
    }
    if (g_host && access(PAIR_FLAG, F_OK) == 0) {
        unlink(PAIR_FLAG);
        host_pair(g_host, PAIR_SECONDS);
        notify(tr(MSG_PAIRING), PAIR_SECONDS);
    }
}

int main(void)
{
    web_cfg wc;
    char ip[16], url[64];
    const char *why = "stop requested";
    long last_power = 0;

    mkdir(STATE_DIR, 0755);
    log_open(LOG_PATH);
    log_line("PadBridge PS5 %s - developed by %s", PADBRIDGE_VERSION, PADBRIDGE_AUTHOR);
    i18n_load(LANG_PATH);
    log_line("language: %s", i18n_code(i18n_get()));
    (void)g_version_tag;
    (void)g_author_tag;

    if (!lock_take(LOCK_PATH)) {
        log_line("another instance is running");
        notify("%s", tr(MSG_ALREADY_RUNNING));
        log_close();
        return 1;
    }
    unlink(STOP_FLAG);                  /* an old request is not for us */
    syscall(SYS_thr_set_name, -1, "padbridge");    /* how others find us */
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    signal(SIGHUP, on_signal);
    signal(SIGPIPE, SIG_IGN);           /* a browser closing early */


    /* The menu first: it is what explains everything that follows. */
    memset(&wc, 0, sizeof wc);
    wc.host = &g_host;
    wc.stop = &g_web_stop;
    wc.retry = &g_retry;
    wc.bt_state = &g_bt_state;
    wc.bt_reason = g_bt_reason;
    wc.bt_reason_key = &g_bt_reason_key;
    wc.lang_path = LANG_PATH;
    wc.log_path = LOG_PATH;
    wc.version = PADBRIDGE_VERSION;
    wc.press_ps = web_press_ps;
    g_web = web_start(&wc, WEB_PORT);

    local_ip(ip);
    snprintf(url, sizeof url, "http://%s:%d", ip, WEB_PORT);
    if (g_web)
        notify(tr(MSG_READY), PADBRIDGE_VERSION, url);
    else
        notify(tr(MSG_MENU_UNAVAILABLE), PADBRIDGE_VERSION, WEB_PORT);

    sysinfo_log();

    /* The entry in the media row that opens the menu in the browser. Done
     * before the credentials are raised for the pads, as Payload Manager does
     * it with its own; /data/padbridge/no_icon skips it, remove_icon undoes it. */
    if (access(RM_ICON_FLAG, F_OK) == 0) {
        apps_remove_launcher();
        unlink(RM_ICON_FLAG);
    } else if (access(NO_ICON_FLAG, F_OK) != 0) {
        apps_install_launcher();
    }

    /* Without virtual pads there is nothing to connect controllers to:
     * Bluetooth is not touched at all, and the menu says why. */
    g_vpad_ok = vpad_init();
    if (!g_vpad_ok) {
        g_bt_state = WEB_BT_FAILED;
        snprintf(g_bt_reason, sizeof g_bt_reason,
                 "Virtual controllers cannot be created on this system (see the log). Bluetooth was not touched.");
        g_bt_reason_key = "novpad";
        notify("%s", tr(MSG_NO_VPAD));
    } else {
        bt_begin();
    }

    while (!g_stop && !g_web_stop) {
        long now;

        if (g_host) host_poll(g_host, 4);
        else usleep(4000);
        now = now_ms();
        web_poll(g_web);
        if (g_vpad_ok) vpad_poll(now);

        /* Rest mode on its way: leave cleanly before it, not after. */
        if (now - last_power >= 200) {
            int ps = power_state();
            last_power = now;
            if (ps == POWER_GOING_TO_REST || ps == POWER_IN_REST) {
                why = "rest mode";
                notify("%s", tr(MSG_REST_MODE));
                break;
            }
        }

        /* The Bluetooth: tries, loss, and the menu's retry button. */
        if (g_bt_state == WEB_BT_TRYING && now >= g_bt_next) bt_try(now);
        if (g_host && host_transport_lost(g_host)) bt_lost("the chip stopped answering");
        if (g_retry) {
            g_retry = 0;
            if (g_vpad_ok && g_bt_state == WEB_BT_FAILED) {
                log_line("retrying Bluetooth at the user's request");
                bt_begin();
            } else {
                log_line("retry ignored (virtual pads not available, or not failed)");
            }
        }

        check_flags(now);
    }
    if (g_web_stop) why = "stop from the menu";
    else if (g_stop && strcmp(why, "stop requested") == 0) why = "stop requested (flag or signal)";

    log_line("stopping: %s", why);
    web_stop(g_web);
    host_close(g_host);                 /* our links; removes our virtual pads */
    g_host = NULL;
    if (g_vpad_ok) {
        vpad_creds_restore();           /* leave the credentials as found */
    }
    lock_release();
    log_line("stopped");
    log_close();
    notify("%s", tr(MSG_STOPPED));
    return 0;
}
