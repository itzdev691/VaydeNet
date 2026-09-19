# VaydeNet Development Status

Snapshot: September 18, 2026

Target bootstrap-branch completion: September 18, 2026

VaydeNet is being developed as a hardware-independent communication framework for embedded systems. The intended application boundary remains independent of ESP-NOW, nRF24L01, LoRa, Bluetooth, Wi-Fi, Ethernet, and future transports.

The active implementation checkpoint is the ESP32 node bootstrap. It can retrieve hardware identity, read a minimal configuration from NVS, select ESP-NOW, initialize a board-specific packet activity LED, apply the configured Wi-Fi channel, initialize ESP-NOW, register a receive callback, log incoming frame metadata, request an LED flash for each exact-sized `Packet` frame, copy that frame into a bounded four-slot FreeRTOS queue, construct an `EngineStartupContext`, hand the initialized dependencies to `VaydeEngine::start()`, and report a specific startup result. After successful startup, the node remains in a FreeRTOS-backed application loop that asks VaydeEngine to process one queued frame every 10 ms. VaydeEngine validates the frame, decodes supported prototype type `1` into a transport-independent `Message`, and dispatches it through a portable `MessageSink`. The current node sink logs logical-message metadata but does not retain, route, relay, acknowledge, or transmit the message.

## Current Startup Path

The current node application follows this path:

```text
ESP-IDF app_main()
    -> NodeBootstrap::run()
    -> retrieveEsp32HardwareIdentity()
    -> initializeEsp32NodeSettingsStorage()
    -> readEsp32NodeSettingsFromStorage()
    -> if empty, write development defaults to NVS
    -> select configured transport
    -> configure ESP-NOW channel
    -> initialize packet activity LED
    -> register packet-receive activity callback
    -> EspNowTransport::initialize()
        -> create four-packet receive queue
        -> register ESP-NOW receive callback
            -> request an LED flash for each exact 220-byte frame
            -> copy exact 220-byte frames into the queue without waiting
    -> construct EngineStartupContext from bootstrap-owned dependencies
    -> VaydeEngine::start()
        -> reject repeated startup
        -> validate board model, protocol version, settings version, and transport selection
        -> retain pointers to identity, settings, transport, and message sink
    -> return and log NodeBootstrapStatus
    -> enter the app_main packet-processing loop
        -> VaydeEngine::processNextPacket()
        -> TransportInterface::tryReceive(Packet&)
        -> validate version, type, TTL, length, and CRC
        -> decode supported packet type into Message
        -> MessageSink::deliver(const Message&)
        -> report processing, validation, decoding, or sink outcome
        -> delay 10 ms before the next receive attempt
```

`NodeBootstrapStatus::Ready` currently means that hardware-identity retrieval, settings loading, transport selection, channel configuration, ESP-NOW initialization, receive-queue creation, receive-callback registration, engine prerequisite validation, and engine dependency binding succeeded. After this status, `app_main()` starts the packet-processing loop. It does not mean that any frame has been dequeued, validated, decoded, or delivered.

`NodeBootstrapStatus::ReadyAfterProvisioning` means the same initialization completed after the loader created and committed the current development defaults: ESP-NOW on channel `1`.

The bootstrap reports distinct results for:

- hardware-identity failure;
- a settings read failure;
- a settings write failure;
- an unsupported transport;
- invalid transport configuration;
- transport initialization failure;
- VaydeEngine startup validation failure;
- successful completion after first-boot provisioning;
- successful completion of the current bootstrap stage.

## Implemented Components

### ESP32 hardware identity

`packages/platforms/esp32/Esp32BoardInfo.cpp` retrieves:

- the default ESP32 eFuse MAC as a six-byte device UID;
- the compile-time `VAYDENET_BOARD_MODEL` value.

Failures are returned through `Esp32BoardInfoStatus`. This identifies the physical board but does not provision a logical node identity.

### ESP32 target environments

`apps/node/platformio.ini` defines node targets for the ESP32-S3 DevKitC-1, ESP32-C5 DevKitC-1, and the ESP32-S2 Flipper Wi-Fi Developer Board. The C5 target uses the pioarduino ESP-IDF 5.5.4-compatible platform package and `apps/node/sdkconfig-c5.defaults` to select 4 MB flash and route logs through USB Serial/JTAG. All targets use a monitor rate of 115200 baud.

The S3 and C5 targets select addressable RGB LEDs on GPIO 48 and GPIO 27. The Flipper S2 target selects the active-low green LED on GPIO 5. Its current `esp32-s2-saola-1` PlatformIO board profile declares 4 MB flash while the shared `sdkconfig.defaults` declares 8 MB, producing a flash-size mismatch warning that must be resolved before treating the S2 profile as final.

### ESP32 packet activity LED

`packages/platforms/esp32/Esp32RgbLed.cpp` provides addressable and active-low GPIO backends behind one `initialize()` and `flash()` interface. A dedicated FreeRTOS task performs the 60 ms flash so the ESP-NOW receive callback does not delay for the LED duration. Accumulated task notifications are cleared together to prevent a packet burst from creating a long delayed flash backlog.

`NodeBootstrap` registers this indicator through an adapter callback. The current callback runs after the incoming frame passes the exact-size check and before queue insertion. The LED therefore indicates radio receipt of a correctly sized frame even when the four-slot receive queue is already full and drops that frame. It does not indicate CRC validation, packet processing, queue consumption, or VaydeEngine delivery. The callback can later be moved to the processing boundary without changing the LED driver.

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

The platform layer can also open the namespace for writing, validate and store `transport`, store `channel`, set `configured` to `1`, and commit the values to flash. Write failures close the NVS handle and return `Esp32SettingsStorageWriteStatus::WriteFailed`.

When the loader finds an unconfigured namespace, it currently creates development defaults for ESP-NOW on channel `1`, writes them once, and continues bootstrap. Later boots read the stored values without overwriting them. This is automatic development provisioning, not an operator-controlled production provisioning interface.

Only `transport` and `channel` are stored and loaded. Node ID, network ID, version fields, capabilities, and security settings retain their in-code defaults.

### Transport boundary

`packages/VaydeEngine/include/VaydeNet/transport/TransportInterface.h` defines portable initialization and nonblocking receive contracts:

```text
TransportInterface::initialize() -> TransportStatus
TransportInterface::tryReceive(Packet&) -> TransportReceiveStatus
```

`TransportReceiveStatus` distinguishes `Received`, `Empty`, and `NotInitialized`. The contract includes the current `Packet` prototype directly so every adapter implementation uses the same complete type.

`EspNowTransport` overrides this method and translates its FreeRTOS queue result into the portable status. Queue ownership, callback registration, and ESP-NOW-specific buffering remain private to the adapter. `VaydeEngine::processNextPacket()` calls this contract and maps transport, validation, decoding, and sink outcomes into `EngineProcessResult` without importing FreeRTOS or ESP-NOW headers into the engine. Raw `Packet` values remain internal to the engine and never cross the bootstrap boundary.

### VaydeEngine startup handoff

`packages/VaydeEngine` is now an ESP-IDF component containing the portable `VaydeEngine` class. `NodeBootstrap` owns the engine alongside the hardware identity, settings, selected transport, and concrete `NodeMessageSink`. After successful transport initialization, bootstrap constructs the non-owning `EngineStartupContext` reference bundle and passes it to `VaydeEngine::start()`.

The engine rejects repeated startup, a missing board model, unsupported protocol or settings versions, and an unspecified transport. After validation, it retains pointers to the bootstrap-owned dependencies and reports `EngineStartStatus::Ok`. Bootstrap maps any rejected start to `NodeBootstrapStatus::EngineStartupFailed`.

`VaydeEngine::start()` remains dependency validation and binding only. After startup, the node application's existing FreeRTOS `app_main` task repeatedly calls `VaydeEngine::processNextPacket()`. The engine performs one nonblocking transport receive attempt per call. A received frame is removed from the queue, validated, decoded into a `Message`, and delivered through `MessageSink`. `EngineProcessResult` preserves the validation outcome without exposing raw packet data. The node's concrete sink logs logical-message metadata but does not retain or relay traffic.

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

The receive callback rejects null metadata, null source addresses, null payload pointers, and non-positive lengths. For accepted callbacks, it logs the sender MAC address and received byte count. It then accepts only frames whose length equals `sizeof(Packet)`, copies them into a local `Packet`, requests the configured activity indication, and uses nonblocking `xQueueSend(..., 0)` to copy them into the receive queue. A full queue causes the new packet to be dropped and logged rather than blocking the Wi-Fi callback; the activity indication has already been requested because it currently represents receipt rather than queue acceptance or processing.

`tryReceive(Packet&)` uses nonblocking `xQueueReceive(..., 0)` and distinguishes `Received`, `Empty`, and `NotInitialized`. VaydeEngine calls it through the portable interface from the node's packet-processing loop. Polling starts only after successful bootstrap and occurs every 10 ms, allowing queued frames to be drained outside the Wi-Fi callback. The adapter does not register peers, transmit data, register a send-completion callback, or perform fragmentation and reassembly.

## Existing Packet and Experiments

### Legacy 220-byte packet

`packages/VaydeEngine/include/VaydeNet/packet/Packet.h` still defines a packed 220-byte structure with version, type, flags, TTL, length, sender ID, sequence number, a 200-byte payload, and CRC.

This is the current prototype packet used by ESP-NOW examples. It is not the finalized universal wire format for every transport. The current validation checkpoint defines a provisional CRC, supported version, nonzero type and TTL requirements, and payload-length bound for this prototype. Canonical message types, flag meanings, TTL processing beyond rejecting zero, wire byte order, acknowledgements, authentication, and duplicate suppression remain undefined or unimplemented.

### ESP-NOW sender and receiver

`apps/examples/esp-now/` contains standalone Arduino/PlatformIO sender and receiver prototypes. They transmit the packed 220-byte `Packet` directly. The sender now emits packets compatible with the current VaydeEngine validator: version `1`, prototype type `1`, TTL `1`, the exact message-byte length, and CRC-16/CCITT-FALSE recalculated after each sequence-number increment. The sender configuration includes four-megabyte ESP32-S3 Zero and ESP32-C5 targets plus an ESP32-S2 Flipper sender target. These examples remain isolated hardware tools rather than the reusable ESP-NOW adapter used by `apps/node`.

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
- hardware validation of controlled packet rejection, sustained queue draining under load, and logical-message sink delivery;
- a common serialized VaydeNet message contract;
- finalized protocol identity, message types, capability discovery, or authentication;
- transport-native nRF24L01, LoRa, Bluetooth, Wi-Fi, or Ethernet adapters;
- persistent inbox storage or application-level handling after logical-message delivery;
- a finalized ESP32-S2 flash-size configuration and hardware proof of its active-low activity LED behavior;
- direct hardware capture of the blank-NVS automatic provisioning branch;
- bidirectional delivery proof and validated VaydeNet message processing.

## Validation

On August 30, 2026, the current milestone source built successfully for both node PlatformIO environments:

- `espnow_esp32c5` under ESP-IDF 5.5.4;
- `espnow_esp32s3` under ESP-IDF 5.5.4.

Both builds compiled and linked the node bootstrap, settings loader, ESP32 NVS storage, hardware-identity component, and ESP-NOW adapter with the receive callback. The C5 image used 40,724 bytes of RAM and 847,490 bytes of flash. The S3 image used 36,488 bytes of RAM and 725,729 bytes of flash.

The standalone sender also built successfully for both `esp32s3_sender` and the new four-megabyte `esp32s3_zero_sender` environment. Each sender image used 43,464 bytes of RAM and 680,241 bytes of flash.

After flashing the ESP32-C5, the user reported `Bootstrap ready`. That demonstrates that configured NVS settings were read, the stored ESP-NOW channel passed validation and was applied during transport initialization, Wi-Fi station mode started, `esp_now_init()` succeeded, and the receive callback registered successfully.

On August 30, 2026, the ESP32-C5 serial monitor repeatedly logged `ESPNOW RX sender=3c:0f:02:e5:4f:50 bytes=220` while the separate ESP32-S3 sender was transmitting. This is direct hardware proof that the configured node bootstrap reached an operational ESP-NOW receive callback, that the devices were aligned on a working radio channel, and that 220-byte frames crossed from the sender to the C5. The callback currently logs before interpreting the payload, so this does not prove that the bytes form a valid VaydeNet `Packet`, that CRC or fields are valid, that frames are buffered safely, or that VaydeEngine received a logical message.

The available serial evidence does not directly demonstrate the blank-NVS writer branch because the distinct `Bootstrap ready; default node settings written to NVS` result was not captured after an isolated erase. It also does not demonstrate transmission from the reusable adapter or bidirectional delivery.

The packet-layout syntax check and `git diff --check` passed after the implementation and documentation changes. These builds did not flash either board during this review; the receive-path runtime evidence is the serial capture supplied by the user.

On August 31, 2026, the bounded-queue changes built successfully for `espnow_esp32c5` and `espnow_esp32s3` under ESP-IDF 5.5.4. The C5 image used 40,732 bytes of RAM and 847,912 bytes of flash; the S3 image used 36,496 bytes of RAM and 726,061 bytes of flash. `git diff --check` also passed. These builds prove compilation and linking of queue creation, callback copying, cleanup, and `tryReceive()`; they do not prove runtime enqueue/dequeue behavior because neither board was flashed for this change.

On September 1, 2026, the minimal `EngineStartupContext` declaration passed a standalone C++17 syntax check, and both node PlatformIO environments built successfully. No node source constructs or includes the context yet, so those firmware builds demonstrate no regression in the existing node targets rather than engine-context integration. No board was flashed for this declaration-only checkpoint.

On September 2, 2026, the current source built successfully for `espnow_esp32s3`, `espnow_esp32c5`, and `esp32-s2` under ESP-IDF 5.5.4. The S3 image used 37,320 bytes of RAM and 750,181 bytes of flash. The C5 image used 41,588 bytes of RAM and 871,106 bytes of flash. The S2 image used 33,376 bytes of RAM and 696,410 bytes of flash after cleaning a stale PlatformIO duplicate-target artifact. The S2 build still reported the 4 MB board-profile versus 8 MB SDK configuration mismatch. The new `esp32c5_sender` environment also built successfully, and the packet-layout test plus the standalone `EngineStartupContext` header syntax check passed. `git diff --check` passed. No firmware was flashed during this review, so the packet-triggered LED behavior remains unverified on hardware.

On September 3, 2026, the `espnow_esp32s3_mini` node environment built successfully with the new VaydeEngine component registration and active bootstrap handoff. The build compiled `main.cpp`, `NodeBootstrap.cpp`, and `VaydeEngine.cpp`, archived `libVaydeEngine.a`, and linked the firmware. The image used 36,712 bytes of RAM and 739,449 bytes of flash. `git diff --check` passed. No firmware was flashed, so this proves compilation and linking of the handoff but not runtime execution, packet consumption, or engine processing.

On September 8, 2026, the portable receive-contract translation built successfully for the `espnow_esp32s3_mini` node environment under ESP-IDF 5.5.4. The image used 36,712 bytes of RAM and 739,497 bytes of flash. `git diff --check` passed, and no `EspNowReceiveStatus` references remained. No firmware was flashed, so this proves that `TransportInterface` and `EspNowTransport` compile and link with the shared `TransportReceiveStatus`; it does not prove runtime queue dequeue, packet validation, or engine delivery.

On September 8, 2026, a host regression test compiled the production `EspNowTransport.cpp` against deterministic ESP-IDF and FreeRTOS fakes, then drove the registered ESP-NOW callback. Address and undefined-behavior sanitizers passed while the test verified exact-size rejection, owned frame copies, FIFO order, four-frame capacity, full-queue dropping, and nonblocking `NotInitialized`, `Empty`, and `Received` results. A clean `espnow_esp32s3_mini` firmware rebuild also passed with 36,712 bytes of RAM and 739,497 bytes of flash. This is deterministic software verification of the adapter logic; it is not physical radio, ESP-IDF scheduler, or hardware enqueue/dequeue proof.

On September 8, 2026, the portable VaydeEngine receive-consumer test passed under address and undefined-behavior sanitizers. It verified rejection before engine startup, one transport receive attempt per poll, and correct mapping of empty, received, and uninitialized transport results. A clean `espnow_esp32s3_mini` firmware build then passed with the application receive loop, using 36,712 bytes of RAM and 739,969 bytes of flash. This proves compilation and deterministic consumer control flow; no board was flashed, so continuous hardware queue draining and serial reporting remain unverified.

On September 12, 2026, the sender was updated to populate every field required by the current validator and to recalculate CRC after each sequence-number change. All four sanitizer-backed host tests passed. The ESP32-S3, S3 Mini, S3 Zero, and ESP32-S2 Flipper sender targets compiled successfully. The ESP32-C5 sender target stopped before source compilation because PlatformIO could not resolve its Arduino framework directory and raised `TypeError: expected str, bytes or os.PathLike object, not NoneType`; this is not C5 build proof.

After flashing compatible sender and node firmware, the user confirmed that node serial output reported an accepted VaydeEngine packet with the expected type, length, and sequence fields. Under the code present at that time, this was hardware/runtime evidence of a 220-byte ESP-NOW frame reaching the node callback, entering and leaving the receive queue, passing version, type, TTL, length, and CRC validation, returning through the former `EngineReceiveResult`, and reaching the application log. This historical proof predates the logical-message decoder and sink and therefore does not prove the current delivery path on hardware.

On September 18, 2026, the sanitizer-backed host suite passed packet layout, packet validation, VaydeEngine processing, logical-message decoding and dispatch, sink rejection, unsupported-type rejection, decoder length rejection, and ESP-NOW receive-queue tests. `git diff --check` passed. A clean `espnow_esp32s3_mini` build compiled `NodeMessageSink.cpp` and `PacketMessageDecoder.cpp`, archived them into the firmware components, and linked successfully. The image used 36,728 bytes of RAM and 741,925 bytes of flash. This proves deterministic host behavior and firmware integration only; the new decoder-to-sink path has not been flashed or observed on hardware.

## Repository State

The active development branch observed on September 18, 2026 is `itzdev691/packet-processing`. It includes the portable receive contract, ESP-NOW adapter translation, host receive-queue regression test, VaydeEngine validation and logical-message dispatch stage, node packet-processing loop, and compatible ESP-NOW sender. `apps/node/dependencies.lock` remains target-sensitive and may change when another node environment is built, so this does not establish a stable multi-target lock policy.

`EngineStartupContext` is constructed from bootstrap-owned hardware identity, node settings, the selected initialized transport, and `NodeMessageSink`. `VaydeEngine::start()` validates and retains those dependencies. The application loop asks the engine to process queued frames; the engine validates, decodes, and dispatches supported messages without returning raw packet data through bootstrap. Persistent message retention remains absent.

The packet-validation checkpoint spans the engine, node application, compatible ESP-NOW sender, host tests, build registration, and this status document. The validator header, source, and focused unit test are included as tracked source for the checkpoint.

The branch contains the cumulative node-bootstrap implementation relative to `main`, including bootstrap ownership, ESP-NOW initialization and receive buffering, VaydeEngine startup and packet consumption, packet validation, supporting examples, tests, and documentation. The bounded feature is ready to merge as a prototype node-bootstrap receive-and-validation checkpoint: software tests pass, firmware integration builds, and the accepted-packet path has user-confirmed hardware/runtime proof. The missing core work and later development sequence below remain follow-on work rather than blockers for this feature boundary.

## Merge Readiness

The current software boundary ends after VaydeEngine dequeues one packet, validates version, type, TTL, length, and CRC, decodes supported prototype type `1` into a `Message`, and dispatches it through `MessageSink`. It includes node-side logging of delivered logical-message metadata and explicit validation, decoding, and sink outcomes. It does not claim hardware proof of the new path, message retention, reusable-adapter transmission, peer management, authentication, routing, relay behavior, production provisioning, or finalized cross-transport serialization.

## Implemented Checkpoint: Decode and Dispatch Validated Packets

The bounded implementation extends the transport-independent packet-validation stage inside `VaydeEngine`. It begins after `TransportInterface::tryReceive(Packet&)` returns `Received`, validates the packet, decodes supported prototype type `1` into an in-memory `Message`, and dispatches it through `MessageSink`. Raw packet data remains internal to the engine. It does not include inbox storage, ESP-NOW peer management, transmission, acknowledgements, duplicate suppression, routing, relay behavior, fragmentation, or a replacement wire format.

### Prototype validation rules

The current 220-byte prototype uses these minimal rules:

- version `1` is the only supported packet version;
- packet type `0` is invalid and every nonzero prototype type is accepted;
- TTL `0` is invalid and every nonzero prototype TTL is accepted;
- `length` is the payload-byte count and must not exceed the 200-byte payload capacity;
- CRC uses CRC-16/CCITT-FALSE with initial value `0xFFFF`, polynomial `0x1021`, and coverage of the 218 packed bytes preceding `Packet::crc`;
- `flags`, `senderID`, and `sequenceNumber` validation is deliberately deferred.

The CRC field is compared directly with the computed `std::uint16_t` value; a transport-independent wire-byte-order contract remains future protocol work. The ESP-NOW sender now populates version, type, TTL, length, and CRC according to these prototype rules. Other examples that leave required fields at zero are rejected.

### Ownership and file changes

1. `PacketValidation.h` defines the portable validation statuses and pure validation API.
2. `PacketValidation.cpp` implements structural checks followed by CRC verification without ESP-NOW, ESP-IDF, FreeRTOS, or board-specific dependencies.
3. The VaydeEngine component registration compiles the validator into host and firmware targets.
4. `PacketMessageDecoder` converts validated prototype type `1` packets into transport-independent `Message` values and excludes the wire CRC.
5. `VaydeEngine::processNextPacket()` retains raw packets internally and returns an `EngineProcessResult` containing only processing status and validation status.
6. `NodeBootstrap` owns `NodeMessageSink`, connects it through `EngineStartupContext`, and forwards only the engine processing result.
7. `apps/node/src/main.cpp` retains the 10 ms scheduling loop and handles processing failures without interpreting raw packet data.
8. `EspNowTransport` remains unchanged. Its callback owns callback-argument checks, exact frame-size checks, copying, queue insertion, and receive-activity notification only.

### Test and completion evidence

The sanitizer-backed host suite covers valid packets, every packet-validation rejection status, payload lengths `0`, `200`, and `201`, CRC corruption, logical-message field and payload decoding, unsupported logical types, sink rejection, exact validation-reason propagation, output clearing on decoder failure, and exactly one receive attempt per engine poll. Empty, unavailable, and pre-start outcomes report `NotChecked`, and raw packet data is absent from the public engine result.

On September 11, 2026, all four sanitizer-backed host tests passed, `git diff --check` passed, and a clean `espnow_esp32s3_mini` firmware build compiled and linked the production validator and accepted-packet return path into `libVaydeEngine.a`. The image used 36,712 bytes of RAM and 740,717 bytes of flash. This completes the software checkpoint and proves deterministic validation behavior plus firmware integration; it does not prove physical queue draining, radio behavior, packet retention, or logical-message delivery.

Historical user-confirmed node output proves the compatible sender exercised the physical ESP-NOW link, receive queue, and validator before the logical-message boundary was added. Current hardware proof still requires flashing this checkpoint and observing `NodeMessageSink` output, controlled failures for each rejection reason, and sustained traffic showing that the four-slot queue continues to drain without unacceptable loss. Packet retention remains later work.

## Later Development Sequence

1. Correct the ESP32-S2 flash-size configuration, then flash and verify that GPIO 5 is off while idle and flashes only for correctly sized received frames.
2. Define a stable dependency-lock policy for the multi-target node project; the current single `dependencies.lock` target changes according to the last environment built.
3. Erase or isolate the NVS namespace and capture the first-boot `ReadyAfterProvisioning` result, then reboot and capture the stored-settings `Ready` result.
4. Exercise missing-key, invalid-transport, invalid-channel, valid-settings, NVS-read-failure, and NVS-write-failure paths.
5. Replace automatic development defaults with an operator-controlled production provisioning contract before deployment.
6. Add a bounded persistent inbox or application handler behind `MessageSink`; the current implementation logs delivered messages only.
7. Add ESP-NOW peer management, transmission, and send-completion handling behind the adapter boundary.
