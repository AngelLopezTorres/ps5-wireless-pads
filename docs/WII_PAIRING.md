# Wii Remote pairing and the Guitar Hero guitar

A Wii Remote pairs with the PS5's internal Bluetooth chip like any other
Classic pad. A few things are specific to it.

## Pairing

- **Press the red Sync button** under the battery cover while PadBridge is
  searching. Pairing with 1+2 (a temporary link) is not implemented.
- **The name decides the PIN.** Before a new Classic connection, PadBridge
  asks the pad's name (with the inquiry's page scan mode and clock offset,
  up to 3 s). "Nintendo RVL-CNT-01" and "Nintendo RVL-CNT-01-TR" select Wii
  bonding. Every other pad keeps the "0000" PIN fallback and Secure Simple
  Pairing as before.
- **The PIN is the host's address.** For red Sync, the PIN is the console's
  own Bluetooth address as six binary bytes in HCI wire order (already the
  reverse of display order), zero bytes included. An unknown all-zero host
  address is refused. Logs show the choice, the PIN length and the
  authentication status, never PIN or key bytes.
- **The HID channels are opened early.** A Wii Remote that is being paired
  waits for the host to open its control and interrupt channels, and drops
  the link if that does not happen within a few seconds. PadBridge waits for
  encryption, then opens them at once, retrying with a fresh channel id if
  the remote answers "pending" or not at all.
- **The radio is kept free for the link.** Page scan is turned down to the
  standard interval while a pad is paged or linked, and searching stops
  once a pad is paired. A frequent page scan left a Wii Remote's reports
  only half the radio, and it disconnected.

The ACL endpoint the PS5's MediaTek chip needs for any of this is described
in [PS5_ACL_OUT.md](PS5_ACL_OUT.md).

## The remote and its guitar

The remote's own buttons map to the virtual DualSense: d-pad, A cross,
B circle, 1 square, 2 triangle, + Options, - Create, Home PS. The four
player LEDs show the console's player number.

When a Guitar Hero guitar is plugged into the remote, PadBridge switches
the extension to unencrypted reports, reads its id (00 00 A4 20 01 03) and
maps it as PlayStation guitars report: green cross, red circle, yellow
triangle, blue square, orange L1, strum on the d-pad, + Options, - (Star
Power) Create, the stick on the left stick and the whammy on the right
stick's x. Tilt needs the accelerometer and is not mapped yet. Drums and
other extensions are recognised but not mapped.

## XInput for Windows games

The virtual DualSense makes a guitar look like a gamepad. A Windows game run
through [prospero-win](https://github.com/mpereiraesaa/prospero-win) reads
XInput, which can tell a guitar apart. So every pad that is a guitar is also
sent, on every change and every 100 ms otherwise, as an XInput guitar (the
RedOctane X-plorer's identity) to UDP 127.0.0.1. The format is in
`src/xinput_feed.h`. Nothing listening costs nothing; prospero-win's runtime
reads it whenever a game's profile has `[input] mode = xinput`.

Tested on PS5 firmware 5.50 with a Wii Remote (RVL-CNT-01) and a Guitar Hero
World Tour guitar: pairing with red Sync, reconnecting, and a full song in
YARG through prospero-win.

Protocol references: https://wiibrew.org/wiki/Wiimote and
https://wiibrew.org/wiki/Wiimote/Extension_Controllers/Guitar_Hero_(Wii)_Guitars
