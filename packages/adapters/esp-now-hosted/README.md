# ESP32-P4 ESP-NOW host component

This ESP-IDF component compiles the pinned ESPHome bridge in
`packages/third_party/esp-now-hosted/host/`. It supplies the missing `esp_now_*`
definitions on ESP32-P4 and forwards operations over ESP-Hosted's CustomRpc.
Other ESP32 targets register an empty component and keep native ESP-NOW.

The component pins ESP-Hosted 2.12.8 and Wi-Fi Remote 1.6.5. Add its directory to
the application's `EXTRA_COMPONENT_DIRS` and require `esp-now-hosted` from the
ESP-NOW adapter only when `IDF_TARGET` is `esp32p4`.

Enable these settings in the P4 application's SDK configuration:

```ini
CONFIG_ESP_HOSTED_ENABLE_PEER_DATA_TRANSFER=y
CONFIG_ESP_HOSTED_MAX_CUSTOM_MSG_HANDLERS=8
```

`EspHomeLogCompat.h` supplies the upstream logging-tag macro. The upstream source
and shared protocol header remain unchanged, retaining their recorded checksums
and GPLv3 license. Original licenses are included in the third-party directory.

The C6 must run the matching companion overlay. A successful P4 build does not
establish a working coprocessor connection or ESP-NOW radio delivery. Follow the
downloaded companion instructions for the ESP-Hosted 2.x overlay workflow.

Runtime boundaries inherited from the upstream bridge include asynchronous
add/delete peer operations: successful submission does not confirm that the C6
applied the change. Receive/send callbacks run on the Hosted RPC receive task
and must not call blocking bridge functions from that task. Hardware startup,
error handling, and peer delivery still require validation.

On October 11, 2026, fresh full node builds from isolated application copies
against repository package sources passed for P4 and C5 using ESP-IDF 5.5.4.
The P4 ELF and link map confirm the bridge definitions are present. The C5 build
excludes bridge compilation and both P4 managed dependencies. No hardware was
flashed, and the companion overlay remains unbuilt.
