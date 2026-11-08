# ACL out on the PS5's MediaTek chip (0e8d:3603)

Interface 0 of the console's internal Bluetooth chip lists two bulk OUT
endpoints, 0x01 and then 0x02. The usual rule, the first one in descriptor
order, picks 0x01. On this chip only 0x02, the OUT paired with bulk IN 0x82,
is drained by the firmware: 0x01 takes one packet and then refuses every other
one. A Classic link therefore stalled after its first ACL packet, the first
L2CAP request. The existing fallback moved to 0x02 only once a later write had
waited 500 ms, about 5.5 s after the first request.

Seen on firmware 5.50 with a Wii Remote: bonding completed (name, binary PIN,
key stored, authentication 0, encryption on), then the log showed "first ACL
packet written on endpoint 0x1", two unanswered SDP channel requests, and
"ACL out 0x1 never finished; switching to 0x2". A Wii Remote whose input
channels are not opened within about 5 s closes the connection (BlueZ
plugins/autopair.c notes this), so it never connected. The same chip behaviour
is documented for ps5-linux, where btusb is patched to send ACL on 0x02:
https://github.com/WizGery/ps5-mediatek-fixes (patch 0003).

Change: on 0e8d:3603, when the HCI function's bulk IN is 0x82 and its second
bulk OUT is 0x02, ACL is written to 0x02 from the first packet, with no spare.
Falling back to 0x01 could only stall again. The log says so at start-up:
"usb: ACL out 0x2, not the first bulk OUT 0x1: this chip drains only 0x2".
Every other chip keeps the first bulk OUT and its spare, as before. Commands
(control endpoint), the event and ACL reads, and their counts are unchanged.

Tested on firmware 5.50: a Wii Remote pairs and its HID channels open, and
its reports arrive. Not addressed here: the same chip can deliver ACL data
before the HCI event that precedes it (ps5-mediatek-fixes patch 0004). That
matters when a pad opens its own channels, as a reconnecting Wii Remote
does.
