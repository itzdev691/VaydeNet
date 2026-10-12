# ESP-NOW over ESP-Hosted: downloaded upstream sources

Downloaded on October 11, 2026 for VaydeNet's ESP32-P4 with onboard ESP32-C6.
Files are unmodified upstream snapshots. SOURCE_MANIFEST.json pins the source
revisions, URLs, sizes, and SHA-256 checksums. SHA256SUMS covers every downloaded file.

- host/: P4 implementations of esp_now_* and the shared RPC protocol header,
  from https://github.com/esphome/esphome/tree/254690100c4768e592017c47550fbf8dbcce69ef/esphome/components/esp32_hosted.
- slave-overlay/: matching C6 ESP-NOW RPC handlers, component files, and upstream
  instructions, from https://github.com/esphome/esp-hosted-firmware/tree/e4c39e0187db8fb6f9921cf8fa875463a792ce7f/slave-overlay.
- apply-overlay.sh: upstream helper for injecting the overlay into an ESP-Hosted
  2.x slave project. Downloaded only; it has not been executed.
- UPSTREAM_README.md: companion firmware build instructions.
- licenses/: original license files from both upstream repositories.

The ESPHome license assigns GPLv3 to its C++ runtime sources and headers.
The companion firmware repository uses Apache-2.0. These downloaded files keep
their upstream licenses; VaydeNet's root license does not replace them.

The host and companion copies of esp_now_hosted_rpc.h are byte-identical.
Both halves are required: host definitions address P4 linking, and matching
C6 firmware supplies the actual radio operations.

## Integration boundary

The P4 source is consumed by the local ESP-IDF wrapper component at
`packages/adapters/esp-now-hosted/`, registered in the node application's CMake
build. The P4 SDK defaults enable Hosted peer data transfer and eight custom
message handlers. The wrapper provides ESPHome's logging-tag macro while keeping
the downloaded files unchanged.

The downloaded C6 overlay remains an unmodified source snapshot and was not
built locally. On October 11, 2026, the agent flashed ESPHome's prebuilt C6
v2.12.13 image, which contains the ESP-NOW overlay and the same protocol header.
The published image checksum and complete flashed-image digest passed; the
existing bootloader, partition table and settings were preserved. The agent's
post-flash capture still showed ROM download mode. Subsequently, on October 11,
2026, the user confirmed working heartbeat sending with this companion and
the installed P4 Hosted 2.12.8 host. The agent did not capture that runtime
event; receiving-peer delivery and sustained traffic remain unverified.
Board details and flash boundaries are in
`apps/node/ESP32-P4-WIFI6-POE-ETH.md` and `DEVELOPMENT_STATUS.md`.

For the currently selected ESP-Hosted 2.x, follow the upstream apply-overlay.sh
instructions; the overlay's standalone CMake component is intended for 3.x.
The wrapper supplies the P4-only build registration, include paths, and pinned
ESP-Hosted dependencies. The upstream bridge's asynchronous peer operations
still need validation against VaydeNet's expected error handling on hardware.
