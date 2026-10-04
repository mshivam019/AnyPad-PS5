# Redgear direct USB for PS5

Based on sinfiltros/AnyPad-PS5 (GPL-3.0-or-later). Tested October 4, 2026 on
firmware 9.00. The user confirmed controller input works with the receiver plugged
directly into the PS5. Console logs confirm USB reports and virtual-pad binding
to the foreground signed-in user. Individual games, rumble, controller power-off, and USB hotplug have
not yet been verified.

## Use without a laptop

1. Jailbreak normally and open Payload Manager.
2. Plug the Redgear receiver into the PS5; turn the controller on in XInput mode.
3. Launch `Redgear-USB.elf` once. Wait for “Redgear USB connected”.

Installed at `/data/pldmgr/payloads/redgear/Redgear-USB.elf`. Refresh Payload
Manager if needed. This is a manual launch, not an autoload entry. It needs no
laptop, network sender, or internet. Relaunch after a reboot or rest mode.
A second launch exits while the first instance is running.

A/B/X/Y map to Cross/Circle/Square/Triangle. Back+Start sends PS;
Back+left-stick-click sends touchpad click. Sticks, D-pad, shoulder buttons and
analog triggers are mapped. Rumble, motion, and touch coordinates are not implemented.

Supported receiver: Redgear `045e:028e`, Xbox 360 interface `ff/5d/01`.
The alternate HID mode `2563:0580` is not supported. The payload scans USB device
descriptors, opens only the matching receiver, and discovers its interrupt endpoints.
It does not detach system drivers or touch the internal Bluetooth transport.
A steady player-one LED command is sent on connect.

Inputs are held between USB reports because this receiver may report only changes.
USB disconnect/errors release controls. Controller radio loss while the receiver
stays plugged in still needs testing; unplug the receiver if controls stick.

Logs: `/data/redgear/usb.log`. Create `/data/redgear/stop` to stop the bridge.
The network and USB versions share a lock and cannot run simultaneously.
User binding defaults to the foreground signed-in user; optional override file
`/data/redgear/bind_user`. If Remote Play uses another user, preserve that assignment.

## Build and checks

Official ps5-payload-sdk v0.43 was used:

```sh
make -f usb/usb.mk PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc -Iusb usb/test_report.c -o /tmp/redgear-usb-tests
/tmp/redgear-usb-tests
```

Parser tests cover neutral input, button mapping, trigger buttons, stick extremes,
Y inversion, shortcut chords and rejected malformed reports. The initial console
trial exposed `USB_FS_COMPLETE` returning EBUSY while transfers are pending; the
working build handles that as no completed input, as FreeBSD specifies.
The installed ELF was read back and byte-compared, and its Payload Manager listing
was verified. See `dist/Redgear-USB.elf.sha256` for the shipped checksum.
