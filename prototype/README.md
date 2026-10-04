# Redgear PS5 network prototype

Local prototype built on sinfiltros/AnyPad-PS5 (GPL-3.0-or-later).
This branch reuses its virtual DualSense code; upstream authors retain attribution.
The original root README describes upstream AnyPad, not this network prototype.

**Current route:** Redgear controller → its receiver plugged into the Linux laptop
→ Linux `xpad` joystick input → UDP on the home network → PS5 payload → virtual DualSense.
The laptop must remain running. Plugging the receiver directly into the PS5 is
not supported by this prototype. Menu input was confirmed on firmware **9.00**. Direct USB is now available on the
`feature/redgear-usb` branch; this network prototype remains for reference.

## What was identified

Receiver manufacturer `hongjingda`, product `Redgear`, USB ID `045e:028e`,
Xbox 360-compatible input, interrupt IN endpoint `0x81` and OUT endpoint `0x02`.
Linux delivered 176 real button/stick events during the initial hardware check.
Earlier enumeration also showed HID mode `2563:0580`; that mode has not been tested here.
The USB strings do not identify the retail model.

## Try it

1. Connect the Redgear receiver to the **laptop**, switch on the controller, and check:

   ```sh
   cd AnyPad-PS5
   python3 prototype/sender.py --dry-run --seconds 15
   ```

   Move the sticks and press buttons. `events` should increase. No extra Python packages are needed.

2. On the jailbroken PS5, create/sign in a second local user. Upstream AnyPad reports
   that games may not see the virtual pad when it shares the DualSense's user.
   To prefer the second signed-in user, use FTP to write `other` into
   `/data/redgear/bind_user` before launching the payload (create the directory if needed).
   An explicit hexadecimal signed-in user ID is also accepted. Otherwise the upstream
   code chooses the initial/foreground signed-in user. User routing still needs testing.

3. Import `dist/Redgear-PS5-network-prototype.elf` into your payload loader and launch it once.
   This is an ELF payload, not an installable PKG. Do not run upstream AnyPad at the same
   time during this experiment. A notification should say it is listening on UDP 8096.

4. Run the sender with your console's LAN address:

   ```sh
   python3 prototype/sender.py --ps5 192.168.1.123
   ```

   Replace the example address. Both devices must be on the same LAN, with UDP 8096 allowed.
   `PS5 receiving` confirms the network path; `virtual pad READY` confirms that the
   payload created, identified and bound the virtual device. Neither alone proves
   that a particular game accepts it. Use Back+Start to send the PS button and select
   the intended console user if prompted.

5. First test console menus, then a game. Ctrl+C stops the sender and requests orderly
   payload shutdown. If the sender dies, all buttons/sticks release after 250 ms.
   The neutral virtual pad remains until shutdown; restart the sender to reconnect.

Mappings: A→Cross, B→Circle, X→Square, Y→Triangle; shoulders, sticks and triggers
map to their equivalents; Back→Create, Start→Options; Back+Start→PS;
Back+L3→touchpad click. No touch movement, gyro, adaptive triggers or rumble.
Input is sent at up to 125 Hz; end-to-end latency has not been measured.

## Diagnose / stop

Retrieve `/data/redgear/bridge.log` via FTP. It records virtual-pad initialization,
the actual API return codes, user IDs, device identification, and timeouts.
Upstream virtual-device identification requires a kernel log stream on localhost
TCP 3232 or readable `/dev/klog`. A missing or ambiguous device announcement
prevents binding; it never guesses a device ID.

```sh
python3 prototype/sender.py --ps5 192.168.1.123 --stop
```

Alternatively create `/data/redgear/stop` via FTP. It is removed on orderly shutdown.
The payload stops on a detected transition to rest mode and restores process credentials.
Firmware-specific behavior, including rest-state detection, remains unverified.
Frames require the matching local pairing token and a private-network source.
This is a prototype LAN protocol, not encrypted transport.

## Build / checks

Use the official ps5-payload-dev SDK. This build used v0.43; its downloaded archive
SHA-256 was `a9cc9929f21b2b2c5d5b309f3bab4997067c45281c0622cf4838b1aecba66fcb`.

```sh
python3 prototype/create_pairing.py
make -f prototype/bridge.mk PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
python3 -m unittest discover -s prototype -p test_sender.py
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -Isrc -Iprototype prototype/test_protocol.c -o /tmp/redgear-protocol-test
/tmp/redgear-protocol-test
make build/test_vpad
./build/test_vpad
```

The generated `pairing.json` and console ELF must stay matched. Tokens are not committed;
run the generator and rebuild for another clone. Keep the existing license and upstream
attribution when publishing a fork. Run only the specific Redgear ELF above: the preexisting
AnyPad ELFs in `dist/` are upstream Bluetooth releases.

Remaining checks: individual-game routing, rest mode, and latency.
