# MicroPython VaydeEngine module

This builds the portable C++ VaydeEngine into MicroPython as `_vaydenet`.
The Python `vaydenet.py` adapter connects it to MicroPython's existing `network`
and `espnow` modules. Application code uses logical payloads and decoded messages;
packet construction, validation, CRC, sender conversion, and sequence state stay
in the production C++ engine.

The normal VaydeNet node `.bin` is not used. Neither `packages/node` nor the
standalone ESP-IDF `EspNowTransport` is linked into this firmware: they initialize
Wi-Fi themselves, while MicroPython owns Wi-Fi and ESP-NOW in this integration.

## Build for ESP32-C5

Use MicroPython v1.29.0 and a supported ESP-IDF release. This module is built with
C++17. MicroPython v1.29.0 supports ESP-IDF 5.5.4, matching the existing node SDK.
Keep the upstream MicroPython and ESP-IDF checkouts outside this repository.

With ESP-IDF installed for `esp32c5`, activate it in the build terminal:

```sh
source /path/to/esp-idf/export.sh
```

Clone MicroPython into your development directory:

```sh
git clone --depth 1 --branch v1.29.0 https://github.com/micropython/micropython.git
```

From the cloned `micropython` directory, set the path to the VaydeNet checkout
and build:

```sh
vaydenet_repo=/Volumes/ExtVault/VaydeNet
make -C mpy-cross -j8
make -C ports/esp32 BOARD=ESP32_GENERIC_C5 \
    USER_C_MODULES="$vaydenet_repo/packages/bindings/micropython/micropython.cmake" \
    submodules
make -C ports/esp32 BOARD=ESP32_GENERIC_C5 \
    USER_C_MODULES="$vaydenet_repo/packages/bindings/micropython/micropython.cmake" \
    -j8
```

The output is `ports/esp32/build-ESP32_GENERIC_C5/firmware.bin`. It is a combined
MicroPython firmware image, with `_vaydenet` built in. The C5 combined image is
flashed at `0x2000`; it differs from the standalone node application's
`firmware.bin` at `0x10000`.

Installing a firmware with a different partition layout can invalidate existing
board files. Copy your Python application files off the board before changing
firmware. Building this module does not flash or erase a device.

## Python application

After installing the combined firmware, upload
`packages/bindings/micropython/vaydenet.py` to the board as `vaydenet.py`.
The native module itself is already in the firmware. A minimal application is:

```python
import time
import vaydenet

vaydenet.init(channel=1)
print(vaydenet.send(b"hello"))
while True:
    message = vaydenet.recv()
    if message is not None:
        print(message["source"], message["payload"])
    time.sleep_ms(10)
```

`apps/examples/micropython-node/main.py` supplies the existing five-second
heartbeat and continues polling after invalid received packets or a heartbeat
radio exception. Failed heartbeat attempts wait for the next five-second interval;
receive polling and GPIO pulse expiry continue. Its application
logs scroll on a 128x64 I2C OLED at address `0x3C`, with SDA on GPIO 2
and SCL on GPIO 3. Upload that script as `/main.py` and the included display
driver to `/display/sh1106.py` or `/display/ssd1306.py`, matching
`OLED_CONTROLLER`. The normal `/lib` driver location is also supported; the
VaydeNet adapter belongs at `/lib/vaydenet.py`. Connect the OLED's VCC to 3.3 V
and GND to the board's GND. Display controller, dimensions and address are
constants at the top of the script.

`OLED_ROTATION = 180` corrects the reported upside-down orientation; set it to
`0` for the opposite mounting orientation. Color inversion is disabled for lit
text on a dark background. Text uses a four-pixel margin on all sides and wraps
at 15 characters, showing seven rows on a 128x64 screen. This keeps complete
8x8 glyphs inside the configured display bounds; older rows scroll off the top.

`OLED_CONTROLLER` currently selects `SH1106` as a diagnostic alternative for
the reported scrambled SSD1306 output. The hardware controller is unconfirmed.
SH1106 uses page addressing and a two-column offset; the SSD1306 driver uses
horizontal addressing. Both commonly use `0x3C`, so an I2C scan does not identify
the controller. Set `OLED_CONTROLLER = "SSD1306"` for a confirmed SSD1306 module.
The SH1106 driver is vendored from
[robert-hh/SH1106](https://github.com/robert-hh/SH1106), with its MIT notice retained.

GPIO 24 starts LOW and goes HIGH when `vaydenet.recv()` returns a valid
message, then goes LOW 500 ms after the latest valid receive. Each valid receive
restarts the pulse timer. Empty polls and rejected packets do not assert it or
extend the pulse. Polling continues during the pulse. The example uses the
native VaydeEngine C5 integration above; OLED and GPIO access use MicroPython's
hardware APIs.
Boot messages and uncaught exception tracebacks remain on the serial console.

## API and ownership

- `init(channel=1)`: activate station Wi-Fi and ESP-NOW, register the broadcast
  peer, initialize engine identity from the station MAC, and reset engine state.
- `send(payload, ttl=1, flags=0)`: broadcast a bytes-like payload, up to 200 bytes.
  Returns `Sent` or `Failed` from synchronous radio completion. Other engine
  failures raise `OSError`; radio exceptions propagate after releasing the send.
- `recv()`: process at most one received radio frame. Returns `None` or a message
  dictionary containing `version`, `type`, `flags`, `ttl`, `source`, `sequence`,
  and lossless bytes `payload`. Non-220-byte frames are ignored; semantic packet
  rejection raises `ValueError` with engine status and validation code.
- `deinit()`: deactivate ESP-NOW and clear engine state. Station Wi-Fi remains
  active for the application to manage.

One engine and one radio session are supported. Call this API from one
MicroPython task; do not share it across `_thread` callers, interrupt handlers,
or radio callbacks. Initialize it again when your application starts after a
soft reset. There is no background engine task. Long application pauses can
overflow MicroPython's radio receive buffer; `recv()` must be polled regularly.

Standalone mode requires an unassociated station interface. Access-point Wi-Fi,
ESP-NOW channel changes, and concurrent use of a separate `espnow.ESPNow()`
session require application coordination. This prototype has no unicast,
authentication, retries, acknowledgements, or routing. `Sent` does not prove
that another VaydeNet application received the message.

The private `_vaydenet` functions exchange packets with the transport adapter.
Application code should use `vaydenet`, which hides those packets.

## Host verification

The Make fragment supports testing the native module with MicroPython's unix
port. From the upstream MicroPython directory:

```sh
vaydenet_repo=/Volumes/ExtVault/VaydeNet
make -C ports/unix -j8 \
    USER_C_MODULES="$vaydenet_repo/packages/bindings/micropython" \
    MICROPY_PY_SSL=0 MICROPY_PY_BTREE=0 MICROPY_PY_FFI=0 FROZEN_MANIFEST=
ports/unix/build-standard/micropython \
    "$vaydenet_repo/tests/micropython/engine_binding_test.py"
ports/unix/build-standard/micropython \
    "$vaydenet_repo/tests/micropython/radio_adapter_test.py" \
    "$vaydenet_repo/packages/bindings/micropython"
ports/unix/build-standard/micropython \
    "$vaydenet_repo/tests/micropython/native_allocation_test.py"
python3 "$vaydenet_repo/tests/micropython/example_loop_test.py"
```

The native test exercises binary round-trip delivery through the actual engine,
sender identity, sequence ownership, CRC rejection, queue capacity, transmit
completion, and reinitialization. It is host software evidence; it does not
establish C5 radio or peer-device behavior. The second test runs the Python
adapter against the real native engine with a fake radio, including recovery
after a radio exception and rejection of a corrupt packet.

The allocation regression runs in a fresh interpreter and verifies native send
statuses while the MicroPython heap is locked, then completes another send.
Native status results are interned strings, so returning a status cannot fail
allocation after the engine has queued a request. The CPython example-loop test
simulates a failed heartbeat, continued receive polling, a 500 ms GPIO pulse,
and the next scheduled heartbeat; it does not exercise physical peripherals.

Verification on September 30, 2026: the native MicroPython host tests and the
seven repository host tests passed. A MicroPython v1.29.0 ESP32_GENERIC_C5 build
with ESP-IDF 5.5.4 and Python 3.11 compiled, linked, and generated the combined
firmware. The application image was 1,939,040 bytes with 92,576 bytes left in its
partition (about 5%). The locally saved combined image is
`apps/examples/micropython-node/build/firmware.bin`; that build directory is
ignored by Git. No firmware was flashed, and radio/peer behavior remains
unverified.

References: [MicroPython external C/C++ modules](https://docs.micropython.org/en/v1.29.0/develop/cmodules.html)
and [ESP32 build instructions](https://github.com/micropython/micropython/blob/v1.29.0/ports/esp32/README.md).

Verification on October 4, 2026: a fresh native unix-module build passed the two
integration suites and the heap-lock regression. The example-loop simulation
and all seven C++ host tests passed. The updated native module compiled and
linked for `ESP32_GENERIC_C5`; its application image is 1,939,616 bytes, leaving
92,000 bytes in its partition. The previously saved combined image above remains
the September 30 artifact and does not contain the status-allocation fix. No
updated firmware was flashed or tested over the radio in this verification.
