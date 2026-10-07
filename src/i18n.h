/* The console notifications in the language chosen on the web page.
 *
 * Eleven languages (including Hebrew), English first and the default. The choice is a two-letter
 * code kept in one small file (/data/padbridge/language) and set through
 * POST /api/lang?code=xx. Every translation takes the same printf arguments,
 * in the same order, as the English text (checked by test_i18n). */
#ifndef PADBRIDGE_I18N_H
#define PADBRIDGE_I18N_H

enum {
    MSG_CONNECTED,          /* %s model, %d slot */
    MSG_CONNECTED_NOVPAD,   /* %s model, %d slot */
    MSG_DISCONNECTED,       /* %s model, %d slot */
    MSG_BT_READY,           /* %d paired */
    MSG_BT_NO_ANSWER,
    MSG_BT_LOST,
    MSG_PAIRING,            /* %d seconds */
    MSG_ALREADY_RUNNING,
    MSG_READY,              /* %s version, %s menu address */
    MSG_MENU_UNAVAILABLE,   /* %s version, %d port */
    MSG_NO_VPAD,
    MSG_REST_MODE,          /* paused, not stopped: resumes after waking */
    MSG_STOPPED,
    MSG_CONTROLLER,         /* the word used when the model is unknown */
    MSG_COUNT
};

#define LANG_COUNT 11

/* 0..LANG_COUNT-1 for a known code ("en", "ar", ...), else -1. */
int         i18n_index(const char *code);
const char *i18n_code(int lang);

/* The current language (0 = English until set or loaded). */
int         i18n_get(void);
void        i18n_set(int lang);

/* Text of `msg` in the current language. */
const char *tr(int msg);
/* ... in a given one (tests). */
const char *tr_in(int lang, int msg);

/* Reads the code saved at `path` and makes it current. Returns the language,
 * 0 if the file is missing or not understood. */
int         i18n_load(const char *path);
/* Saves the current language at `path`. Returns 1 on success. */
int         i18n_save(const char *path);

#endif
