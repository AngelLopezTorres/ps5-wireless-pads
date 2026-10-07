<p align="center"><img src="assets/logo.png" width="140" alt="ps5-wireless-pads"></p>

<h1 align="center">ps5-wireless-pads</h1>

<p align="center">
  <b>Use Xbox, Switch Pro, DualShock 4 and generic Bluetooth controllers on a jailbroken PS5.</b><br>
  <sub>No dongle. No adapter. Just the console's own Bluetooth chip.</sub>
</p>

<p align="center">
  <img alt="License: GPL-3.0-or-later" src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue">
  <img alt="Platform: PS5" src="https://img.shields.io/badge/platform-PS5-003791">
  <img alt="Payload: ELF" src="https://img.shields.io/badge/payload-ELF%20%C2%B7%20port%209021-555">
  <img alt="Status: beta" src="https://img.shields.io/badge/status-beta-orange">
</p>

<p align="center">
  <a href="#what-it-does">What it does</a> ·
  <a href="#whats-new-in-this-fork">What's new</a> ·
  <a href="#controllers">Controllers</a> ·
  <a href="#quick-start">Quick start</a> ·
  <a href="#pair-a-controller">Pairing</a> ·
  <a href="#building-from-source">Building</a> ·
  <a href="#known-limits">Limits</a> ·
  <a href="#credits-and-license">Credits</a>
</p>

---

> [!NOTE]
> **ps5-wireless-pads is a fork of [PadBridge PS5](https://github.com/X-F1REBALL-X/PadBridge-PS5)** by
> X-F1REBALL-X, itself based on **AnyPad-PS5** by sinfiltros. All the Bluetooth, pairing and controller work
> comes from those projects. This fork adds rest-mode survival and hot replacement (see
> [What's new](#whats-new-in-this-fork)). On the console the payload still calls itself **PadBridge**.

## What it does

ps5-wireless-pads is an ELF payload that runs in the background on a jailbroken PS5. It pairs Bluetooth
controllers through the console's **built-in Bluetooth chip** and presents each one to the system and to games
as a **virtual DualSense**.

```
 Xbox / Switch Pro / DS4 / generic pad
              │  Bluetooth (Classic or LE)
              ▼
   PS5 internal Bluetooth chip  ──►  payload (HCI over /dev/ugen0.2)
                                          │  per-controller parser
                                          ▼
                              virtual DualSense (scePadVirtualDevice*)
                                          │
                                          ▼
                               PS5 system menu and games
```

- Up to **four controllers at once**, all working simultaneously.
- Does **not** modify games, the firmware or the jailbreak.
- Controlled from a **web page** served by the console itself (port `8095`).

## What's new in this fork

| Change | Before (PadBridge 0.1.9-beta) | Now |
|---|---|---|
| **Rest mode** | The payload exited when the console went to sleep, so it had to be loaded again. | It **pauses** before sleep (controllers disconnect, Bluetooth and the menu are released) and **resumes by itself** a few seconds after waking. Paired controllers reconnect. |
| **Loading a new copy** | Refused with *"already running"*. | The new copy checks that the running process really is PadBridge, asks it to stop (SIGTERM), waits up to 5 s and takes its place. **Updating is just sending the ELF again.** |
| **Signals** | `signal()` | `sigaction()` for TERM, INT and HUP, SIGPIPE ignored. |

The rest-mode and takeover logic is covered by host unit tests (`tests/test_suspend.c`, `tests/test_lock.c`).
The design follows patterns from [ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus).

> [!WARNING]
> These two features have been **tested on the host only, not yet on a real console.** Whether the console's
> Bluetooth chip is available again right after waking is still to be confirmed on hardware. Reports are very
> welcome.

## Controllers

| Status | Controller |
|---|---|
| ✅ Tested on a real PS5 (upstream, firmware 10.20) | **Xbox Series X\|S** over Bluetooth LE (`045e:0b13`) |
| ✅ Tested on a real PS5 (upstream, firmware 10.20) | **DualShock 4 v1 / v2** (`054c:05c4`, `054c:09cc`) |
| 🧪 Supported in code, not tested on a console yet | **Switch Pro** and Switch Online pads, other Xbox One / Elite / Adaptive models, DualSense, 8BitDo, GameSir |
| 🧪 Generic profile | Any Bluetooth HID gamepad, read from its HID descriptor and adjustable with a `.map` file |

Not supported: Xbox One controllers **without** Bluetooth (models 1537 / 1697, they use Microsoft's proprietary
radio) and pads that only work with a 2.4 GHz USB receiver.

The full list with every controller ID is in [COMPATIBILITY.md](COMPATIBILITY.md).

## Firmware support

The payload has no firmware-specific offsets of its own; the kernel offsets come from the
[ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk), which includes firmware **13.60**
(`crt/kernel.c`). What you need is a jailbreak that leaves an ELF loader on port **9021**:

| Firmware | Public exploit with an ELF loader on port 9021 |
|---|---|
| 7.00 – 13.60 | [Relapse](https://github.com/ntfargo/Relapse-Exploit) (WebKit, from the PS5 User Guide browser) |
| 9.00 – 12.40 | [Y2JB / P2JB](https://github.com/matem6/P2JB-Y2JB-Porting) |

> [!IMPORTANT]
> Upstream testing was done on firmware **10.20**. This fork has **not been confirmed on 13.60 yet.**

## Quick start

1. **Jailbreak the console.** For example, with Relapse: set the custom DNS, open
   *Settings → User's Guide, Health and Safety, and Other Information → User's Guide* and load the Relapse
   page. Follow the [Relapse README](https://github.com/ntfargo/Relapse-Exploit).
2. **Send the payload** to the ELF loader from a PC on the same network:

   ```sh
   nc -q0 <ps5-ip> 9021 < PadBridge-PS5-<version>.elf
   # or
   socat -u FILE:PadBridge-PS5-<version>.elf TCP:<ps5-ip>:9021
   ```

3. The console shows a notification with the menu address: **`http://<ps5-ip>:8095/`**. A **PadBridge**
   entry also appears in the media row (home icon).

The jailbreak is not persistent: after a full **reboot** you must run the exploit and send the ELF again.
Rest mode does not require it (see [What's new](#whats-new-in-this-fork)).

## Pair a controller

1. Open the **PadBridge** home icon in the media row, or **`http://<ps5-ip>:8095/`** from a PC or phone.
2. Press **Pair a controller (60 s)**. Do **not** use the PS5's own Bluetooth search in Settings.
3. Put the controller in pairing mode so it **blinks fast**:

   | Controller | How |
   |---|---|
   | Xbox | Turn it on, hold the pair button on top until the logo **blinks fast** |
   | Switch Pro | Hold the sync button next to the USB-C port |
   | DualShock 4 | Hold **Share + PS** until the light bar flashes |
   | Generic pad | Follow its manual for Bluetooth pairing mode |

4. When it is detected, **turn the controller off and on again** (required the first time). The console shows
   e.g. `PadBridge: Xbox Series X|S connected (slot 1)`.

Pairing is stored in `/data/padbridge/pads.db`; afterwards just turn the controller on. To give a controller
without a PS button the console's focus, press **Press PS** on its card in the web page.

### Xbox button mapping

| Xbox | PS5 |
|---|---|
| A / B / X / Y | Cross / Circle / Square / Triangle |
| Menu (≡) | Options |
| View (⧉) | Touchpad click |
| Share | Create |
| Xbox button | PS |

## Stopping and updating

- Closing the page leaves the payload running.
- To stop it: **Stop PadBridge (advanced)** at the bottom of the page (press twice), or create the empty file
  `/data/padbridge/stop`.
- To update: send the new ELF. It replaces the running copy automatically.

## Files on the console

Everything lives in `/data/padbridge/`:

| File | Purpose |
|---|---|
| `padbridge.log` | Log (also shown on the page under **Log**) |
| `pads.db` | Paired controllers and their keys |
| `language` | Language chosen on the page (11 languages) |
| `config.ini` | Optional settings |
| `maps/VVVV_PPPP.map` | Optional button maps for the generic profile (see `maps/EXAMPLE.map`) |
| `pair`, `stop` | Create one of these empty files to start pairing or to stop the payload |
| `no_icon`, `remove_icon` | Do not add the media-row entry / remove it once |

## Web API

Small HTTP API on port **8095**. Every `POST` must carry the header **`X-PadBridge: 1`**.

| Method | Path | What it does |
|---|---|---|
| GET | `/api/state` | Version, Bluetooth state, language, connected and paired controllers (JSON) |
| GET | `/api/log` | End of the log |
| POST | `/api/pair` | Pair for 60 seconds |
| POST | `/api/ps?slot=N` | Press PS on slot N (1-4) |
| POST | `/api/rumble?slot=N` | 500 ms rumble test on slot N |
| POST | `/api/forget?addr=AA:BB:CC:DD:EE:FF` | Forget a paired controller |
| POST | `/api/lang?code=xx` | `en`, `ar`, `he`, `es`, `fr`, `de`, `pt`, `ru`, `ja`, `zh`, `it` |
| POST | `/api/retry` | Retry Bluetooth after a failure |
| POST | `/api/stop` | Stop the payload |

```sh
curl -X POST -H "X-PadBridge: 1" "http://<ps5-ip>:8095/api/rumble?slot=1"
```

> [!CAUTION]
> The web menu listens on **all network interfaces** and has **no password**. Use it only on a trusted
> home network.

## Building from source

Requirements: the [ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk), Clang/LLD (the SDK accepts
LLVM 15–23), `make` and `xxd`.

```sh
make test                                          # host unit tests and simulators (Linux/macOS)
make ps5 PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk  # -> dist/PadBridge-PS5-<version>.elf
```

<details>
<summary><b>Fedora Silverblue / Kinoite (immutable) with toolbox</b></summary>

```sh
toolbox create ps5dev
toolbox run -c ps5dev sudo dnf install -y bash llvm-devel clang lld wget make git file \
    socat cmake meson pkg-config python3 python3-pyelftools xxd
git clone https://github.com/ps5-payload-dev/sdk && cd sdk
toolbox run -c ps5dev make DESTDIR=$PWD/_prefix install
cd ../ps5-wireless-pads
toolbox run -c ps5dev make test
toolbox run -c ps5dev make ps5 PS5_PAYLOAD_SDK=$PWD/../sdk/_prefix
```

</details>

> [!TIP]
> `xxd` is required: without it, `make` silently embeds an **empty** web page into the ELF.

The web page (`web/index.html`) and the icon are embedded in the ELF; `make` regenerates `src/web_page.c` and
`src/icon_data.c` from them. The version is set in `src/version.h`.

## Known limits

- **Rumble from games is not passed to the controller.** Only the 500 ms test from the web page rumbles.
- Joy-Con are not combined into a single controller.
- Up to four bridged controllers.
- Controllers marked 🧪 and the rest-mode feature still need confirmation on real hardware.

## Contributing and reports

Reports are welcome. Please include: controller model, console model and firmware, what worked, and
`/data/padbridge/padbridge.log`.

## Credits and license

- **[PadBridge PS5](https://github.com/X-F1REBALL-X/PadBridge-PS5)** by **X-F1REBALL-X**: the payload this fork is built on.
- **AnyPad-PS5** by **sinfiltros**: the original Bluetooth bridge.
- **[ps5-payload-dev SDK](https://github.com/ps5-payload-dev/sdk)** by John Törnblom and contributors.
- **[ShadowMountPlus](https://github.com/drakmor/ShadowMountPlus)** by drakmor: reference for the rest-mode and takeover patterns.

Licensed under **GPL-3.0-or-later**, see [LICENSE](LICENSE). Third-party notices are in [NOTICE](NOTICE).

This project is not affiliated with or endorsed by Sony Interactive Entertainment, Microsoft or Nintendo.
All trademarks belong to their respective owners.
