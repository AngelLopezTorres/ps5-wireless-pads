# Changelog

Every ELF is `dist/PadBridge-PS5-<version>.elf`, with a `.sha256` beside it. The
version is set in `src/version.h` and is also in the first line of the log, in
the start-up notification, on the web page, and in the binary
(`strings PadBridge-PS5-*.elf | grep padbridge-version`).

## Unreleased

- Rest mode: PadBridge no longer exits. It pauses on the way to rest (pads, virtual pads, Bluetooth and the menu are released; pairings kept) and, once the console has been awake for 5 s, opens them again with tries that back off (2, 4, 8, 16 s). Stopping (flag, signal) still works while paused.

## 0.1.9-beta

- Web UI: new look of its own: dark graphite with lime / ember accents, angular cards and badges, a sidebar with an original "PadBridge" hero graphic (inline SVG), new logo mark and condensed uppercase type.
- Web UI: the live input view is now a drawn controller (original SVG) chosen from the pad's vid:pid: Xbox One / Series X|S / Elite Series 2 / Adaptive, DualShock 4, DualSense / DualSense Edge, PS-layout third-party pads (Hori, Razer Raiju, SCUF), Switch Pro, Switch Online SNES / N64 / Mega Drive, 8BitDo SN30/SF30 Pro, Pro 2/3, Ultimate, GameSir, NVIDIA Shield, Atari VCS, other Android pads and a generic fallback. Each uses its own button labels; presses light up live, sticks move and triggers fill.
- Same features, API, languages (11, Arabic and Hebrew right to left) and credit as before. No changes to Bluetooth, pairing or virtual pads; pairings are kept.

## 0.1.8-beta

- Controllers: add Xbox Series X|S USB id `045e:0b12` (Share already handled) and Adaptive Bluetooth id `045e:0b0c` to the Xbox profile.
- Controllers: add Switch Online Mega Drive 2nd id `057e:201e`.
- Controllers: named + Android-order mappings for more 8BitDo Bluetooth / wireless ids (SF30/SN30 Pro BT, Pro 2/3, Ultimate Wireless / 2 / 2C) and GameSir G7 Pro 8K / Tarantula 8K.
- Host tests: `_DEFAULT_SOURCE` so `clock_gettime` builds on Linux; lock boot-time falls back to `/proc/stat` on Linux (PS5 still uses FreeBSD `sysctl`).
- No changes to LE pairing, virtual pads or the web UI. Pairings from 0.1.7-beta are kept.

## 0.1.7-beta

- Hebrew (עברית) added as an 11th UI language (right-to-left, same as Arabic) on the web menu and in console notifications.
- Docs / Pages how-to site: language picker; explanation text follows the chosen language (English default).
- How-to: removed the “pick a second console user” step (not required).
- Docs: DualShock 4 v1 (`054c:05c4`, CUH-ZCT1) verified on PS5 FW 10.20 (user X-F1REBALL-X). README, COMPATIBILITY.md and Pages how-to only; no ELF change.
- Docs: clarify that up to four bridged controllers can connect at once and all work simultaneously (README, Pages how-to / i18n); no ELF change.

## 0.1.6-beta

- Credit: "PadBridge PS5 - Developed by X-F1REBALL-X" is now shown on the web page (under the title, with a
  link to <https://github.com/X-F1REBALL-X>), on the website and in the README. It is the same in every
  language.
- The first line of the log now reads `PadBridge PS5 <version> - developed by X-F1REBALL-X`, and the binary
  carries `padbridge-author X-F1REBALL-X` beside `padbridge-version`.
- Docs: How-to rewritten to match the real first-use flow — open the PadBridge home icon after loading the
  ELF, press Pair so the controller blinks fast, then power the paired controller off and on once; pairing
  is saved afterwards. After pairing succeeds, turn the DualSense off to play with the bridged pad (hold PS;
  PadBridge does not power it down) — not before pairing. README, website and release notes only; no ELF change.
- No changes to Bluetooth, pairing, button mapping, virtual pads or notifications. Pairings from 0.1.5-beta
  are kept.

## 0.1.5-beta

- Fix: LE controllers (e.g. Xbox Series 0C:35:26:51:F2:56) could not connect, neither to pair nor to
  reconnect. Every LE Create Connection (`0x200D`) was answered `0x0c` (Command Disallowed) and the log
  repeated `found LE gamepad` / `connecting to pair` / `connection refused (status 0x0c)` hundreds of
  times. The controller only allows one LE connection attempt at a time, so `0x0c` means another one
  was still pending: one left by the system's driver, or an orphaned one of ours. PadBridge freed its
  link without cancelling anything, turned scanning back on, and tried again at the next advert, so it
  stayed stuck. Classic (BR/EDR) controllers were not affected.
- LE connection attempts now:
  - turn our LE scan off first and wait for its Command Complete (or 300 ms if the reply went to the
    system's driver) before sending Create Connection;
  - on `0x0c`, send LE Create Connection Cancel (`0x200E`), wait for its Command Complete and for the
    LE Connection Complete with status `0x02` that ends the cancelled attempt (whoever's it was), then try
    again after 300, 800 and 1500 ms. After four refused tries the address rests for 5 s;
  - make at most one attempt per address every 2 s; scanning stays off while an attempt is in progress;
  - only act on the Command Status of our own Create Connection: a refusal arriving when we are not
    waiting for one (the system's) is logged and ignored instead of dropping our link;
  - cancel our pending Create Connection when an unconnected LE link is dropped (closing, forgetting),
    so no orphaned attempt is left in the controller.
- The accept list and the system's settings are not touched; the connection is still made to the
  pad's address directly (filter policy 0).
- New diagnostics in the log, rate limited (at most 12 lines every 10 s): per refused try, the status
  of scan off (`0x200C`), create (`0x200D`) and cancel (`0x200E`), and whether the cancel ended a pending
  attempt and to which address; a final line when an address is given a rest; `connected ... (try N,
  after a cancel)` when a cancel was needed; statuses on `did not connect`; non-zero scan enable/disable
  replies.
- `test_le`: two new runs against a simulated controller with someone else's attempt pending (refused,
  cancelled, connected on the third try; refused on every try, rest, then connected).
- No changes to classic pairing, button mapping, virtual pads, the web page or languages. Pairings from
  0.1.4-beta are kept.

## 0.1.4-beta

- Language option on the web page: a **Language** button (top right, flag of the current language) opens
  two columns of flag + native-name banners for English, العربية, Español, Français, Deutsch, Português,
  Русский, 日本語, 中文 and Italiano - the same ten languages, flags and picker style as Elf Launcher and
  WK Autoloader. Every string of the page is translated (status, pairing, controller cards, paired list,
  log, footer, confirmations and the small pop-up messages). Arabic switches the whole page to right to
  left; sticks, controller ids and addresses keep their left-to-right layout. English is the default.
- The choice is saved on the console: new `POST /api/lang?code=xx` (with `X-PadBridge: 1`) writes
  `/data/padbridge/language`, and `GET /api/state` reports it as `"lang"`. The console's choice wins when
  the page opens; `localStorage` (`padbridge-lang`) is only used for the first paint or when the console
  cannot be reached.
- Console notifications follow the chosen language, still with the full controller model name, e.g.
  `PadBridge: Xbox Series X|S conectado (ranura 1)`. New `src/i18n.c` with the 13 notification texts in
  the ten languages; `test_i18n` checks that every translation takes exactly the same printf arguments as
  English, plus the language codes and the saved file. The language is read at start-up and logged.
- Bluetooth failure reasons on the page are translated too: `/api/state` now also carries a
  `"reasonKey"` (`nochip`, `lost`, `novpad`); the English detail stays in `"reason"` and in the log.
- The console hotkey text uses English button names (it had a few Spanish ones).
- README rewritten in English only (what it is, tested controllers, loading, pairing, Press PS, rumble
  limits, languages, files, the web API, building). COMPATIBILITY.md is English only.
- No changes to Bluetooth, LE, button mapping or virtual pads. Pairings from 0.1.3-beta are kept.

## 0.1.3-beta

- Naming cleanup across the code and docs: identifiers, include guards,
  comments, log lines and paths all say PadBridge (`PADBRIDGE_VERSION`,
  `padbridge_config`, ...). The binary tag is now `padbridge-version` and the
  payload thread is named `padbridge`.
- The web API header is now `X-PadBridge: 1` (server and page). Scripts that
  sent the old header must be updated.
- Data folder: `/data/padbridge/` (log `padbridge.log`, lock `padbridge.lock`,
  `pads.db`, `config.ini`, `maps/`, flag files). Pairing store header is now
  `PADBRG02` (same record layout).
- Media-row entry title id is now `PDBR00001`.
- Updating from 0.1.2-beta: stop the old copy first. Pairings and settings from
  the previous folder are not carried over, so controllers are paired again;
  the previous media-row entry stays until it is deleted on the console.
- Changelog rewritten to cover PadBridge releases only. No Bluetooth, LE,
  mapping or virtual-pad changes.

## 0.1.2-beta

- New visual identity, drawn from scratch: launcher / media-row icon
  (`assets/icon0.png`, also `assets/logo.png`) - a gamepad hanging from a neon
  cyan-to-violet bridge arc on a dark tile. Generated by `tools/make-icon.py`
  (cairosvg + Pillow) and checked with `tools/check-icon.py`;
  `src/icon_data.c` regenerated. The console rewrites the installed icon on the
  next start because it differs.
- Web page rewritten from scratch (new layout and CSS, English only), sized for
  the PS5 browser cursor: status panel (Bluetooth, connected / paired counts,
  Retry when Bluetooth failed), a big "Pair a controller (60 s)" button with a
  countdown bar, one card per connected controller (model, slot P1-P4, live
  sticks / triggers / buttons, battery when reported, Press PS / Rumble /
  Forget), the paired list, a collapsible log that refreshes while open, and
  Stop in the footer. Forget and Stop ask for a second press on the page
  instead of a browser dialog. Cards update in place instead of being rebuilt
  every second.
- Same API as before: `GET /api/state`, `GET /api/log`, `POST /api/pair`,
  `/api/forget?addr=`, `/api/ps?slot=`, `/api/rumble?slot=`, `/api/retry`,
  `/api/stop`. No C changes besides the version and the regenerated embedded
  files; `test_web` expects the PadBridge title.

## 0.1.1-beta

First PadBridge PS5 release.

- Bluetooth controllers on a jailbroken PS5 through the console's own
  Bluetooth chip, each presented to the system and to games as a virtual
  DualSense. Classic (BR/EDR) and Bluetooth LE, with LE Secure Connections.
- Verified on a console: DualShock 4 v2, and Xbox Series X|S over Bluetooth LE
  (`045e:0b13`, firmware 10.20): pairing, input in menus and games.
- Xbox buttons: Menu (≡) = Options, View (⧉) = touchpad click, Share = Create,
  Xbox = PS.
- Web menu on port 8095 (also from the console's media row): pairing for 60 s,
  connected and paired controllers, Press PS (for pads without a PS button),
  Forget, Retry, Stop, and the log.
- Rumble test: `POST /api/rumble?slot=N` (or the Rumble button) pulses the pad
  for 500 ms. Rumble sent by games is not forwarded yet (no known way to read
  it from a virtual DualSense).
- Console notifications are English only, short, and name the controller model
  from its vid:pid, e.g. `PadBridge: Xbox Series X|S connected (slot 2)` /
  `PadBridge: Xbox Series X|S disconnected (slot 2)`. Unknown ids fall back to
  the profile's name, then to "Controller" (`profile_model_name()` /
  `pad_display_name()` in `src/profiles.c`, tested in `test_profiles`).
- Stops by itself before rest mode; one running copy at a time.
