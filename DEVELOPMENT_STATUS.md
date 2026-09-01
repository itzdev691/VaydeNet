# VaydeNet Development Status

Snapshot: August 31, 2026

VaydeNet is being developed as a hardware-independent communication framework for embedded systems. The intended application boundary remains independent of ESP-NOW, nRF24L01, LoRa, Bluetooth, Wi-Fi, Ethernet, and future transports.

The active implementation checkpoint is the ESP32 node bootstrap. It can retrieve board information, read a minimal configuration from NVS, select ESP-NOW, apply the configured Wi-Fi channel, initialize ESP-NOW, register a receive callback, log incoming frame metadata, copy exact-sized `Packet` frames into a bounded four-slot FreeRTOS queue, and report a specific startup result. It does not yet consume or validate queued packets, start VaydeEngine, or exchange validated application messages through the reusable adapter.

## Current Startup Path

The current node application follows this path:

```text
ESP-IDF app_main()
    -> NodeBootstrap::run()
    -> retrieveBoardInformation()
    -> initializeNodeSettingsStorage()
    -> readNodeSettingsFromStorage()
    -> if empty, write development defaults to NVS
    -> select configured transport
    -> configure ESP-NOW channel
    -> EspNowTransport::initialize()
        -> create four-packet receive queue
        -> register ESP-NOW receive callback
            -> copy exact 220-byte frames into the queue without waiting
    -> return and log NodeBootstrapStatus
```

`NodeBootstrapStatus::Ready` currently means that board information, settings loading, transport selection, channel configuration, ESP-NOW initialization, receive-queue creation, and receive-callback registration succeeded. It does not mean that VaydeEngine is running or that an incoming frame has been dequeued or validated as a VaydeNet message.

`NodeBootstrapStatus::ReadyAfterProvisioning` means the same initialization completed after the loader created and committed the current development defaults: ESP-NOW on channel `1`.

The bootstrap reports distinct results for:

- board-information failure;
- a settings read failure;
- a settings write failure;
- an unsupported transport;
- invalid transport configuration;
- transport initialization failure;
- successful completion after first-boot provisioning;
- successful completion of the current bootstrap stage.

## Implemented Components

### ESP32 board information

`packages/platforms/esp32/Esp32BoardInfo.cpp` retrieves:

- the default ESP32 eFuse MAC as a six-byte device UID;
- the compile-time `VAYDENET_BOARD_MODEL` value.

Failures are returned through `BoardInfoStatus`. This identifies the physical board but does not provision a logical node identity.

### ESP32 target environments

`apps/node/platformio.ini` defines node targets for the ESP32-S3 DevKitC-1 and ESP32-C5 DevKitC-1. The C5 target uses the pioarduino ESP-IDF 5.5.4-compatible platform package and `apps/node/sdkconfig-c5.defaults` to select 4 MB flash and route logs through USB Serial/JTAG. Both targets use a monitor rate of 115200 baud.

### Portable node settings contract

`packages/VaydeEngine/include/VaydeNet/config/NodeSettings.h` defines the current portable settings structure:

- logical node and network identifiers;
- protocol and settings-format versions;
- selected transport;
- channel;
- capability flags;
- security mode.

The transport defaults to `TransportType::Unspecified`. The structure contains `std::string` members and must not be serialized as raw memory.

### ESP32 NVS settings read and write path

`packages/platforms/esp32/Esp32NodeSettingsStorage.cpp` initializes NVS and reads the minimal stored configuration from the `vaydenet` namespace.

The implemented keys are:

| Key | Type | Meaning |
| --- | --- | --- |
| `configured` | `uint8_t` | Must equal `1` before settings are accepted |
| `transport` | `uint8_t` | Must map to `EspNow`, `Nrf24`, or `Ethernet` |
| `channel` | `uint16_t` | Passed to the selected transport |

Settings are read into a temporary `NodeSettings` object and assigned only after every required read succeeds. Missing namespace data or a missing/inactive `configured` marker is reported as `NotConfigured`; malformed, missing, or unreadable required values are reported as `ReadFailed`.

The platform layer can also open the namespace for writing, validate and store `transport`, store `channel`, set `configured` to `1`, and commit the values to flash. Write failures close the NVS handle and return `SettingsStorageWriteStatus::WriteFailed`.

When the loader finds an unconfigured namespace, it currently creates development defaults for ESP-NOW on channel `1`, writes them once, and continues bootstrap. Later boots read the stored values without overwriting them. This is automatic development provisioning, not an operator-controlled production provisioning interface.

Only `transport` and `channel` are stored and loaded. Node ID, network ID, version fields, capabilities, and security settings retain their in-code defaults.

### Transport boundary

`packages/VaydeEngine/include/VaydeNet/transport/TransportInterface.h` currently defines a portable initialization contract only:

```text
TransportInterface::initialize() -> TransportStatus
```

It does not yet define send, receive, addressing, discovery, callbacks, or message delivery into VaydeEngine.

`EspNowTransport` now exposes an adapter-specific nonblocking `tryReceive(Packet&)` method, but that method is not yet part of `TransportInterface`. The portable receive contract and its VaydeEngine consumer remain undefined.

### ESP-NOW adapter initialization

`packages/adapters/esp-now/EspNowTransport.cpp` currently performs:

1. configured-channel validation;
2. NVS initialization;
3. network-interface initialization;
4. default event-loop creation;
5. Wi-Fi driver initialization;
6. RAM-backed Wi-Fi storage selection;
7. Wi-Fi station-mode selection and startup;
8. primary-channel application through `esp_wifi_set_channel()`;
9. ESP-NOW initialization;
10. creation of a four-slot FreeRTOS queue sized for the current 220-byte `Packet`;
11. ESP-NOW receive-callback registration.

The accepted configured channel range is `1` through `14`. Initialization is rejected when no channel has been configured. Reinitialization returns success after a successful first initialization.

The receive callback rejects null metadata, null source addresses, null payload pointers, and non-positive lengths. For accepted callbacks, it logs the sender MAC address and received byte count. It then accepts only frames whose length equals `sizeof(Packet)`, copies them into a local `Packet`, and uses nonblocking `xQueueSend(..., 0)` to copy them into the receive queue. A full queue causes the new packet to be dropped and logged rather than blocking the Wi-Fi callback.

`tryReceive(Packet&)` uses nonblocking `xQueueReceive(..., 0)` and distinguishes `Received`, `Empty`, and `NotInitialized`. No application or engine code calls it yet, so an active sender will fill all four slots and subsequent packets will be dropped. The adapter does not yet register peers, transmit data, register a send-completion callback, validate the 220-byte prototype packet, deliver a logical message into VaydeEngine, or perform fragmentation and reassembly.

## Existing Packet and Experiments

### Legacy 220-byte packet

`packages/VaydeEngine/include/VaydeNet/packet/Packet.h` still defines a packed 220-byte structure with version, type, flags, TTL, length, sender ID, sequence number, a 200-byte payload, and CRC.

This is the current prototype packet used by ESP-NOW examples. It is not the finalized universal wire format for every transport. CRC behavior, canonical message types, flag meanings, TTL processing, length validation, acknowledgements, authentication, and duplicate suppression remain undefined or unimplemented.

### ESP-NOW sender and receiver

`apps/examples/esp-now/` contains standalone Arduino/PlatformIO sender and receiver prototypes. They transmit the packed 220-byte `Packet` directly. The sender configuration includes a four-megabyte ESP32-S3 Zero target used to generate the traffic observed by the node receive callback. These examples remain isolated hardware tools rather than the reusable ESP-NOW adapter used by `apps/node`.

### Message inbox simulator

`apps/examples/message-inbox-simulator/` is a hardware-free learning example for logical message delivery into a node inbox. It is not connected to the production packet, transport, bootstrap, or engine code.

### nRF24L01

`apps/examples/nrf24l01/` currently contains PlatformIO configuration only. There is no tracked sender/receiver source implementing a VaydeNet nRF24L01 frame.

### WT32-ETH01

`apps/examples/wt32-eth01/ethernet-smoke/` is a tracked standalone experiment. It monitors LAN8720 Ethernet state, probes configured TCP ports, serves a local dashboard, and broadcasts telemetry as the legacy 220-byte `Packet` over ESP-NOW.

It does not provide a reusable Ethernet transport adapter or connect Ethernet to the node bootstrap.

## Revised Protocol Direction

VaydeNet will not require every transport to carry one identical packed C++ structure or one fixed frame size.

The intended boundary is:

```text
Application message
    -> common VaydeNet identity and message semantics
    -> transport adapter encoding
    -> transport-native frame
```

Each adapter will own its payload limit, addressing, serialization, validation, optional fragmentation and reassembly, and translation back to the common logical message model.

A VaydeNet message is expected to carry enough common information to identify the protocol and version, message type, source node, logical message or sequence, flags, and valid payload length. Exact fields, widths, serialization rules, integrity protection, authentication, routing, and capability identifiers are not finalized.

A protocol identifier recognizes VaydeNet traffic. A source-node identifier identifies the sender. Neither authenticates the sender.

For nRF24L01, a complete logical message should fit into one 32-byte transport frame when possible. Fragmentation should be optional for larger messages rather than mandatory conversion of every message from the legacy 220-byte packet. The exact nRF24L01 frame layout is still design work.

## Missing Core Work

The current checkpoint does not include:

- an operator-controlled production provisioning, update, reset, or migration workflow;
- loading the full `NodeSettings` schema;
- tests for empty, missing, corrupt, valid, and unsupported stored settings;
- ESP-NOW peer management;
- ESP-NOW transmission and send-completion handling;
- a portable receive method on `TransportInterface`, consumption and hardware validation of the new queue, packet validation, and logical-message delivery through the reusable adapter;
- a common serialized VaydeNet message contract;
- finalized protocol identity, message types, capability discovery, or authentication;
- transport-native nRF24L01, LoRa, Bluetooth, Wi-Fi, or Ethernet adapters;
- construction and use of the declared `EngineStartupContext`;
- `VaydeEngine::start()` and the handoff from bootstrap into the engine;
- an operational node-ready loop;
- direct hardware capture of the blank-NVS automatic provisioning branch;
- bidirectional delivery proof and validated VaydeNet message processing.

## Validation

On August 30, 2026, the current milestone source built successfully for both node PlatformIO environments:

- `espnow_esp32c5` under ESP-IDF 5.5.4;
- `espnow_esp32s3` under ESP-IDF 5.5.4.

Both builds compiled and linked the node bootstrap, settings loader, ESP32 NVS storage, board-information component, and ESP-NOW adapter with the receive callback. The C5 image used 40,724 bytes of RAM and 847,490 bytes of flash. The S3 image used 36,488 bytes of RAM and 725,729 bytes of flash.

The standalone sender also built successfully for both `esp32s3_sender` and the new four-megabyte `esp32s3_zero_sender` environment. Each sender image used 43,464 bytes of RAM and 680,241 bytes of flash.

After flashing the ESP32-C5, the user reported `Bootstrap ready`. That demonstrates that configured NVS settings were read, the stored ESP-NOW channel passed validation and was applied during transport initialization, Wi-Fi station mode started, `esp_now_init()` succeeded, and the receive callback registered successfully.

On August 30, 2026, the ESP32-C5 serial monitor repeatedly logged `ESPNOW RX sender=3c:0f:02:e5:4f:50 bytes=220` while the separate ESP32-S3 sender was transmitting. This is direct hardware proof that the configured node bootstrap reached an operational ESP-NOW receive callback, that the devices were aligned on a working radio channel, and that 220-byte frames crossed from the sender to the C5. The callback currently logs before interpreting the payload, so this does not prove that the bytes form a valid VaydeNet `Packet`, that CRC or fields are valid, that frames are buffered safely, or that VaydeEngine received a logical message.

The available serial evidence does not directly demonstrate the blank-NVS writer branch because the distinct `Bootstrap ready; default node settings written to NVS` result was not captured after an isolated erase. It also does not demonstrate transmission from the reusable adapter or bidirectional delivery.

The packet-layout syntax check and `git diff --check` passed after the implementation and documentation changes. These builds did not flash either board during this review; the receive-path runtime evidence is the serial capture supplied by the user.

On August 31, 2026, the bounded-queue changes built successfully for `espnow_esp32c5` and `espnow_esp32s3` under ESP-IDF 5.5.4. The C5 image used 40,732 bytes of RAM and 847,912 bytes of flash; the S3 image used 36,496 bytes of RAM and 726,061 bytes of flash. `git diff --check` also passed. These builds prove compilation and linking of queue creation, callback copying, cleanup, and `tryReceive()`; they do not prove runtime enqueue/dequeue behavior because neither board was flashed for this change.

## Repository State

The active development branch is `agent/esp32-node-bootstrap`. Commit `87ef457` (`Milestone: add bounded ESP-NOW receive queue`) is the current committed checkpoint and matches `origin/agent/esp32-node-bootstrap` as of this snapshot.

The next startup-context checkpoint declares the minimal `EngineStartupContext` reference bundle for bootstrap-owned board information, node settings, and the selected transport. Bootstrap does not yet construct the context or start VaydeEngine.

The branch also contains multiple concerns relative to `main`, including node bootstrap work, the ESP-NOW adapter, example configuration, and the message-inbox simulator. Build success does not make the complete branch merge-ready. Scope cleanup, documentation review, focused commits, settings-path testing, and hardware validation remain required before merge.

## Immediate Development Sequence

1. Erase or isolate the NVS namespace and capture the first-boot `ReadyAfterProvisioning` result, then reboot and capture the stored-settings `Ready` result.
2. Exercise missing-key, invalid-transport, invalid-channel, valid-settings, NVS-read-failure, and NVS-write-failure paths.
3. Replace automatic development defaults with an operator-controlled production provisioning contract before deployment.
4. Flash and exercise the bounded queue, then add controlled `tryReceive()` consumption, packet validation, and delivery outside the Wi-Fi callback context.
5. Add ESP-NOW peer management, transmission, and send-completion handling behind the adapter boundary.
6. Define the logical message contract before connecting message I/O to VaydeEngine.
7. Construct the declared `EngineStartupContext` from bootstrap-owned objects and hand the successfully initialized transport into `VaydeEngine::start()`.
