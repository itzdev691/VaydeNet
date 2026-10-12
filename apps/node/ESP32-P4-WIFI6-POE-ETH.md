# Waveshare ESP32-P4-WIFI6-POE-ETH

The P4 runs the application; its onboard C6 supplies the ESP-NOW radio through
ESP-Hosted over SDIO. The C6 must run an ESP-NOW-enabled Hosted image whose
protocol matches the P4 bridge. The verified ESPHome v2.12.13 prebuilt was
flashed onto this C6; the P4 host dependency remains pinned to 2.12.8.
The user confirmed working heartbeat sending on October 11, 2026.
Receiving-peer delivery and sustained traffic remain unverified.

## Select the silicon profile

| P4 chip revision | PlatformIO environment |
| --- | --- |
| v0.1 through v1.x engineering sample | `esp32p4_c6` |
| v3.0 or later | `esp32p4_c6_rev3` |

The product name does not identify silicon revision. Read the chip revision
from the serial boot log or Espressif's chip identification tool before
flashing. Each environment uses its own SDK file and the matching PlatformIO
chip variant. PlatformIO image headers do not encode the SDK revision limits;
upload acceptance does not confirm a matching chip. Never bypass a revision mismatch with `--force`.

Both environments override the reference board's flash metadata to 32 MB and
identify the board as `Waveshare-ESP32-P4-WIFI6-POE-ETH`.
Activity LED GPIO -1 leaves the indicator unavailable.

## Observed device startup

On October 11, 2026, the user supplied a VaydeNet boot log identifying P4
revision v1.3, ESP-IDF 5.5.4, a 360 MHz CPU and console UART on GPIO37/38.
This device uses `esp32p4_c6`. The P4 entered `app_main()` successfully.

Transport initialization then failed because `esp_wifi_init()` reached the
remote Wi-Fi API before `esp_hosted_init()` had configured the host transport
and RPC layer. The P4 adapter now initializes Hosted first. The disabled LED
sentinel (-1) is also handled without calling a GPIO API. A successful build
of these repairs does not establish a corrected runtime or C6 connection;
the previous log stopped before Wi-Fi and ESP-NOW became operational.
The corrected P4 and native C5 builds passed on October 11, 2026, including
combined image assembly. Rebuild and reflash the pre-v3 P4 environment for
this v1.3 device; the C6 companion remains unbuilt/unflashed by the integration.

## Latest flash and runtime result

On October 11, 2026, the agent identified the device at
/dev/cu.usbmodem5B910669381 as P4 v1.3 with 32 MB flash, clean-built the actual
checkout and uploaded esp32p4_c6. Bootloader, partition table and application
hash verification all passed. The serial port is an observed value and must
be detected again before reuse.

The UART capture matched the new ELF prefix d8b49ec7e. LED-disabled handling
and Hosted init now work; the C6 is identified over SDIO and remote Wi-Fi starts.
ESP-NOW init then times out on Req_CustomRpc (0x184), and NodeBootstrap reports
transport initialization failure. The C6 advertises 0.0.0 and Hosted flags a
version mismatch. The matching C6 overlay remains unbuilt/unflashed; no node
readiness or peer delivery is established. The agent uploaded the P4 only.

## C6 flash checkpoint

On October 11, 2026, Flipper USB-UART reached C6FH8 v0.2 with 8 MB flash.
The agent backed up the first 64 KB and active 1.5 MB ota_0 partition, then
flashed the ESPHome v2.12.13 ESP-NOW application at 0x10000. The full image
digest verified; a separate digest check confirmed the first 64 KB unchanged.
Bootloader, partition table, settings and OTA selector were preserved.
Recovery files are in
/Volumes/ExtVault/VaydeNet-device-backups/c6-20261012T032942Z.

The post-flash capture still showed download mode. Power off the board, remove
C6 IO9-to-GND, release P4 BOOT and power on normally. New application startup,
P4/C6 ESP-NOW initialization and peer delivery had not been verified at that
flash checkpoint.

Subsequently, on October 11, 2026, the user reported "now works, heartbeat
sent." Heartbeat sending is user-confirmed with P4 Hosted 2.12.8 and the C6
ESPHome v2.12.13 companion. No additional P4 dependency upgrade was performed.
The agent did not capture that runtime event; receiving-peer delivery and
sustained traffic remain unverified.

## Board wiring

| Signal | P4 GPIO |
| --- | --- |
| C6 SDIO clock | 18 |
| C6 SDIO command | 19 |
| C6 SDIO data 0 / 1 / 2 / 3 | 14 / 15 / 16 / 17 |
| C6 enable/reset | 54 |
| Console UART0 TX / RX | 37 / 38 |

The schematic confirms these connections. Type-C connects through the CH343P
UART bridge; console output uses UART0 at 115200 baud. Type-A is USB OTG.
Hosted calls the EN-high / reset-low sequence `RESET_ACTIVE_HIGH`.

Shared settings are in `sdkconfig-p4-c6.defaults`. Silicon settings are in
`sdkconfig-p4-pre-v3.defaults` and `sdkconfig-p4-rev3.defaults`.
An existing generated `sdkconfig.<environment>` overrides defaults. The local
pre-v3 SDK file was updated to 32 MB, UART0, eight CustomRpc handlers and
warning-level bootloader logging on
October 11, 2026.

## P4 build and upload

Run from the repository root, selecting the environment matching the chip:

```sh
pio run --project-dir apps/node --environment esp32p4_c6
pio run --project-dir apps/node --environment esp32p4_c6 --target upload --upload-port PORT
pio device monitor --port PORT --baud 115200
```

Replace the environment with `esp32p4_c6_rev3` for revision 3.x.
Connect to Type-C. If automatic download entry fails, hold BOOT, press and
release RESET, then release BOOT. These uploads write the P4 only.

## C6 companion

The pinned sources and overlay workflow are documented in
`packages/third_party/esp-now-hosted/README.md`. The downloaded overlay can
be built into a custom Hosted 2.x companion. Alternatively, ESPHome v2.12.13
provides a prebuilt with the same ESP-NOW protocol header; that image has now
been flashed. The upstream v2.12.8 prebuilt lacks this overlay. Peer data
transfer and eight custom handlers remain required.

Waveshare exposes separate C6 UART pads. Its documented sequence is:

1. Before power-on, connect C6 IO9 to GND.
2. Connect a 3.3 V UART adapter: C6 TX to adapter RX, C6 RX to adapter TX,
   and a common GND.
3. Hold the P4 BOOT button while powering the board to prevent P4 firmware
   from controlling C6 reset.
4. Flash the built companion to the adapter's port. Remove IO9-to-GND before
   normal startup.

A P4-based C6 updater is not implemented in VaydeNet.
The source snapshot was not built locally; the verified v2.12.13 prebuilt
was flashed into the existing C6 application partition.

## Build image boundary

Warning-level bootloader logging keeps both silicon profiles below the partition
table at 0x8000. The initial v3 build linked but its bootloader was 24,736 bytes,
overlapping that table by 160 bytes; PlatformIO reported success despite the
image-merge error. Image assembly and offset checks are therefore required
in addition to compiler/linker success.

Final builds on October 11, 2026 passed for both environments. Their bootloaders
are 22,000 and 22,176 bytes, leaving 2,576 and 2,400 bytes before the table.
Both combined images assembled successfully. No agent flash was performed.

## Factory watchdog report

The user's watchdog excerpt came from the newly received board's factory
firmware. CPU0 ran `main` while IDLE0 missed its watchdog deadline.
The excerpt does not identify the stalled function or establish a VaydeNet
failure. No matching factory ELF or complete boot trace was available.
VaydeNet startup markers and frame pointers are diagnostic preparation,
not a confirmed repair of the factory application.

## Sources

Verified October 11, 2026:

- [Waveshare documentation](https://docs.waveshare.com/ESP32-P4-WIFI6-POE-ETH)
- [Waveshare schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-POE-ETH/ESP32-P4-WIFI6-POE-ETH-Schematic.pdf)
- [Waveshare revision and C6 flashing FAQ](https://docs.waveshare.com/ESP32-P4-WIFI6-POE-ETH/FAQ)
