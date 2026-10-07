# Compatibility

Every controller below has its own entry in the code, listed one by one.

| Symbol | Meaning |
|---|---|
| ✅ | **Verified** on a real PS5 by a person |
| 🧪 | **Supported, not yet tested**: you can use this controller on the PS5; the support is written and passes the simulator tests, but nobody has tried it on a console yet |

**Today: 3 verified controllers, 60 more supported ids, plus the generic profile for any other Bluetooth HID gamepad.**

DualShock 4 v2, DualShock 4 v1 and Xbox Series X|S (Bluetooth LE) verified on PS5 firmware 10.20.

## Sony DualShock 4 family

| USB id | Controller | Status |
|---|---|---|
| `054c:09cc` | Sony DualShock 4 v2 (CUH-ZCT2) | ✅ Verified (PS5 FW 10.20) |
| `054c:05c4` | Sony DualShock 4 v1 (CUH-ZCT1) | ✅ Verified (PS5 FW 10.20) |
| `0f0d:00f6` | Hori Onyx | 🧪 |
| `1532:1009` | Razer Raiju Ultimate | 🧪 |
| `1532:100a` | Razer Raiju Tournament Edition | 🧪 |
| `2e95:7725` | SCUF Vantage 2 | 🧪 |

## Sony DualSense family

| USB id | Controller | Status |
|---|---|---|
| `054c:0ce6` | Sony DualSense | 🧪 |
| `054c:0df2` | Sony DualSense Edge | 🧪 |

## Xbox

| USB id | Controller | Status |
|---|---|---|
| `045e:02e0` | Xbox One S (Bluetooth Classic firmware) | 🧪 |
| `045e:02fd` | Xbox One S (Bluetooth Classic firmware, 2nd id) | 🧪 |
| `045e:0b00` | Xbox Elite Series 2 (Bluetooth Classic firmware) | 🧪 |
| `045e:0b05` | Xbox Elite Series 2 (Bluetooth Classic firmware, 2nd id) | 🧪 |
| `045e:0b0a` | Xbox Adaptive Controller (Bluetooth Classic firmware) | 🧪 |
| `045e:0b0c` | Xbox Adaptive Controller (Bluetooth Classic, 2nd id) | 🧪 |
| `045e:0b12` | Xbox Series X|S (USB id) | 🧪 |
| `045e:0b13` | Xbox Series X|S (Bluetooth LE) | ✅ Verified (PS5 FW 10.20) |
| `045e:0b20` | Xbox One S, current firmware (Bluetooth LE) | 🧪 |
| `045e:0b21` | Xbox Adaptive Controller (Bluetooth LE) | 🧪 |
| `045e:0b22` | Xbox Elite Series 2 (Bluetooth LE) | 🧪 |

Bluetooth LE models need Secure Connections pairing; whether the PS5's chip supports every step is unverified.

## Nintendo Switch

| USB id | Controller | Status |
|---|---|---|
| `057e:2009` | Nintendo Switch Pro Controller (and pads that emulate it) | 🧪 |
| `057e:2017` | Nintendo Switch Online SNES controller | 🧪 |
| `057e:2019` | Nintendo Switch Online N64 controller | 🧪 |
| `057e:201a` | Nintendo Switch Online Mega Drive / Genesis controller | 🧪 |
| `057e:201e` | Nintendo Switch Online Mega Drive / Genesis controller (2nd id) | 🧪 |

Rumble is not done yet for these.

## Third-party pads with a built-in mapping

The layout is read from each pad's own HID descriptor, and corrected where needed.

| USB id | Controller | Status |
|---|---|---|
| `ffff:046e` | GameSir G3s | 🧪 |
| `05ac:022d` | GameSir G3s (alternate mode) | 🧪 |
| `ffff:046f` | GameSir G4s | 🧪 |
| `3537:1022` | GameSir G7 Pro | 🧪 |
| `3537:103c` | GameSir Tarantula 8K | 🧪 |
| `3537:10b8` | GameSir G7 Pro 8K | 🧪 |
| `ffff:0450` | GameSir T1s | 🧪 |
| `05ac:056b` | GameSir T2a | 🧪 |
| `20bc:5501` | Betop 2585N2 | 🧪 |
| `2e2c:0002` | Bionik Vulkan | 🧪 |
| `0079:181c` | LanShen X1Pro | 🧪 |
| `2717:3144` | Xiaomi Mi Controller | 🧪 |
| `1949:0402` | Amazon Fire / iPega controller | 🧪 |
| `2dc8:2100` | 8BitDo SN30 Pro for Xbox Cloud Gaming | 🧪 |
| `2dc8:2101` | 8BitDo SN30 Pro for Xbox Cloud Gaming (2nd id) | 🧪 |
| `2dc8:3012` | 8BitDo Ultimate 2.4G | 🧪 |
| `2dc8:301b` | 8BitDo Ultimate 2C Wireless | 🧪 |
| `2dc8:3106` | 8BitDo Ultimate Wireless | 🧪 |
| `2dc8:6001` | 8BitDo SN30 Pro | 🧪 |
| `2dc8:6003` | 8BitDo Pro 2 | 🧪 |
| `2dc8:6006` | 8BitDo Pro 2 (Bluetooth) | 🧪 |
| `2dc8:6009` | 8BitDo Pro 3 | 🧪 |
| `2dc8:6012` | 8BitDo Ultimate 2 Wireless | 🧪 |
| `2dc8:6100` | 8BitDo SF30 Pro (Bluetooth) | 🧪 |
| `2dc8:6101` | 8BitDo SN30 Pro (Bluetooth) | 🧪 |
| `2dc8:ab20` | 8BitDo Pro 2 (older id) | 🧪 |
| `1949:0403` | iPega 9-series | 🧪 |
| `05ac:022c` | iPega 9-series (alternate id) | 🧪 |
| `1038:1412` | SteelSeries | 🧪 |
| `0111:1420` | SteelSeries (2nd id) | 🧪 |
| `0111:1431` | SteelSeries (3rd id) | 🧪 |
| `0111:1419` | SteelSeries (4th id) | 🧪 |
| `0955:7214` | NVIDIA Shield controller (2017) | 🧪 |
| `1532:0900` | Razer Serval | 🧪 |
| `20d6:89e5` | PowerA MOGA Hero | 🧪 |
| `20d6:0dad` | PowerA MOGA Pro | 🧪 |
| `20d6:6271` | PowerA MOGA Pro 2 | 🧪 |
| `3250:1002` | Atari VCS Modern controller | 🧪 |
| `2e24:200a` | Hyperkin Scout | 🧪 |
| `04e8:046e` | Mocute 050 | 🧪 |

## Any other Bluetooth HID gamepad

The generic profile reads the pad's HID descriptor and applies a default mapping (Android/Xbox or DirectInput order). Adjust it with a `.map` file (see `maps/EXAMPLE.map`).

## Not supported yet

DualShock 3, Wii Remote / Wii U Pro, Joy-Con used alone.

## Help us verify

Tried one? Open an issue with the model, the console firmware, what worked and `/data/padbridge/padbridge.log`.
