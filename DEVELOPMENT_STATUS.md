# VaydeNet Development Status

Snapshot: August 22, 2026

VaydeNet is being developed as a hardware-independent communication framework for embedded systems. Its application-facing interface is intended to work across ESP-NOW, nRF24L01, LoRa, Bluetooth, Wi-Fi, Ethernet, and future transports.

This document separates the revised protocol direction from code that currently exists. The revised protocol is not implemented yet.

## Revised Protocol Direction

VaydeNet will not require every radio to transmit one identical binary packet or one fixed packet size.

The universal part of VaydeNet will be a shared protocol identity and message model. Each transport adapter will encode that information in a frame suited to its radio.

```text
Application message
    -> VaydeNet identity and message metadata
    -> transport adapter
    -> ESP-NOW, nRF24L01, LoRa, Bluetooth, Wi-Fi, or Ethernet frame
```

This replaces the earlier assumption that every transport must carry the current packed 220-byte `Packet` unchanged.

### VaydeNet identity

Every VaydeNet message must carry enough common information to answer these questions:

- Is this a VaydeNet message?
- Which protocol version does it use?
- What kind of message is it?
- Which VaydeNet node sent it?
- Which logical message does it belong to?
- How many payload bytes are valid?

The common metadata is expected to include:

- a VaydeNet protocol identifier or signature;
- a protocol version;
- a message type;
- a source-node identifier;
- a sequence number;
- flags;
- a payload length.

Network identity, destination identity, routing lifetime, integrity, and authentication fields still require design decisions. The final field sizes and serialized layout are not defined.

The protocol identifier identifies VaydeNet traffic. The source-node identifier identifies the device that sent the message. These are separate concepts.

A protocol identifier is not proof that a sender is trusted. Authentication and message integrity require a separate mechanism.

### Purpose-independent communication

Devices do not need to perform the same job to communicate through VaydeNet. They need a compatible VaydeNet protocol version, a compatible transport profile, and at least one message type they both understand.

VaydeNet interoperability has three levels:

1. **Recognition:** a device can identify a VaydeNet message.
2. **Transport:** a device can receive, relay, fragment, or reassemble the message.
3. **Interpretation:** a device understands the message's application payload.

A gateway may relay a message without understanding its payload. A display may accept sensor-data messages while ignoring motor-control messages. Nodes will eventually advertise capabilities so differently purposed devices can discover which messages and services they support.

Initial general message categories are expected to include discovery, capability announcement, data, command, acknowledgement, error, and fragment messages. These categories are design targets, not implemented protocol values.

## Transport-Native Frames

Each adapter owns its radio-facing frame layout. Radio frames do not need to be byte-for-byte identical across transports.

The adapter is responsible for:

- adding or encoding the required VaydeNet identity;
- fitting the message into the transport's payload limit;
- using transport-native addressing where appropriate;
- fragmenting messages only when required;
- reassembling fragmented messages before delivery;
- validating lengths, versions, and integrity information;
- translating the received frame into the common VaydeNet message model.

This means transport independence exists at the VaydeNet API and message-semantics boundary, not at the physical-frame boundary.

### nRF24L01 direction

An nRF24L01 frame can be a complete VaydeNet message. It no longer needs to be one fragment of a mandatory 220-byte packet.

The current design target is a packed 32-byte transport frame containing a compact VaydeNet header and a small application payload. One illustrative allocation is:

```text
32-byte nRF24 frame
    12-byte VaydeNet identity/control header
    20-byte application payload
```

The 12-byte and 20-byte split is not a finalized ABI. It demonstrates that common sensor readings, status changes, button events, commands, acknowledgements, and discovery messages can fit into one radio transmission.

Fragmentation remains available as an optional message type:

- a message that fits the transport payload is sent as one complete frame;
- a larger message is divided into fragment frames;
- the receiving adapter reassembles and validates the original message;
- applications and VaydeEngine do not manipulate radio fragments directly.

The earlier design in which a 220-byte packet always became 22 nRF24 fragments is no longer the default protocol model.

### Other transports

ESP-NOW, LoRa, Bluetooth, Wi-Fi, and Ethernet adapters may use different frame sizes and different transport-specific metadata. They must preserve the required VaydeNet identity and message meaning.

A transport with more capacity may carry a larger VaydeNet payload in one frame. A constrained transport may use a compact payload, fragmentation, streaming, or an application-specific message sequence.

## Current Implementation

### Existing 220-byte packet

`packages/VaydeEngine/include/VaydeNet/packet/Packet.h` currently defines a packed 220-byte structure containing version, type, flags, TTL, length, sender ID, sequence number, a 200-byte payload, and CRC.

Compile-time validation currently fixes this structure at 220 bytes. It represents the previous packet model and is still used by the ESP-NOW examples. It has not yet been replaced by the revised VaydeNet identity and transport-native framing model.

The existing structure must not be described as the final universal wire format. Migration or replacement requires a separate implementation change after the common message contract and transport profiles are defined.

### ESP-NOW experiment

`apps/examples/esp-now/` contains sender and receiver prototypes that transmit the existing 220-byte `Packet` directly.

These examples prove only the earlier direct-structure experiment. They do not implement the revised protocol identifier, capability discovery, finalized message types, serialization rules, validation policy, or a reusable ESP-NOW adapter.

### nRF24L01 experiment

`apps/examples/nrf24l01/` currently contains local project configuration and generated development artifacts, but the current working tree does not contain an end-to-end sender and receiver implementation of the revised 32-byte VaydeNet frame.

The nRF24L01 transport still needs:

- a finalized compact frame layout;
- a VaydeNet protocol identifier;
- sender and receiver implementations using the same frame type;
- payload-length and protocol-version validation;
- optional fragmentation and reassembly for larger messages;
- hardware verification;
- migration into a reusable adapter after the prototype is validated.

### Ethernet experiment

The local `apps/examples/wt32-eth01/` area currently contains generated build and editor-index artifacts. The current branch does not contain tracked WT32-ETH01 source code. Ethernet transport support must not be treated as implemented on this branch.

### ESP32 board information

`packages/platforms/esp32/` contains local ESP32 board-information work that can:

- read the default hardware MAC from the ESP32 eFuse;
- use it as the initial device UID;
- retrieve the compile-time board model;
- report UID and board-model failures through `BoardInfoStatus`.

This work is not yet part of a completed VaydeEngine startup sequence.

### Node settings scaffold

`packages/VaydeEngine/include/VaydeNet/config/NodeSettings.h` now defines an initial portable settings structure containing:

- logical node and network identifiers;
- protocol and settings-format versions;
- an explicitly unspecified transport selection and channel;
- capability flags;
- a security mode.

`apps/node/src/NodeSettingsLoader.h` declares the application-side loading contract. Its current `.cpp` implementation returns `NodeSettingsLoadStatus::NotConfigured`; it does not read NVS, apply defaults, validate stored data, or populate `NodeSettings` yet.

The loader source and VaydeEngine public include path are registered in the node component. A PlatformIO ESP32-S3 build completed successfully after registration, confirming the new source and headers compile. This is compile validation only, not settings-loading or hardware validation.

### Node application scaffold

`apps/node/` contains local ESP-IDF bootstrap work:

```text
ESP-IDF
    -> app_main()
    -> NodeBootstrap::run()
    -> retrieveBoardInformation()
```

`NodeBootstrap` currently owns a `BoardInformation` object and stops when retrieval fails. It does not yet own a `NodeSettings` object or call `loadNodeSettings()`. It also does not report startup failures, create an engine startup context, initialize a transport, start VaydeEngine, or enter a node-ready state.

## Planned Software Boundary

The intended startup and ownership boundary remains:

```text
ESP-IDF app_main()
    -> NodeBootstrap
    -> EngineStartupContext
    -> VaydeEngine::start()
    -> selected transport adapter
```

Responsibilities are divided as follows:

- `app_main()` enters the application bootstrap.
- `NodeBootstrap` retrieves physical board information, loads logical node settings, and creates and connects concrete platform dependencies.
- The node application owns settings-loading policy; ESP32-specific persistence belongs in the ESP32 platform package.
- `EngineStartupContext` exposes those dependencies to the portable engine.
- VaydeEngine works with logical VaydeNet messages rather than physical radio frames.
- `packages/platforms/esp32/` implements ESP32-specific hardware operations.
- `packages/adapters/` will contain transport-specific encoding, framing, validation, fragmentation, and reassembly.
- Applications define which message types and capabilities they understand.

`EngineStartupContext`, `VaydeEngine::start()`, the common message API, capability discovery, routing, configuration interfaces, and reusable transport adapters are not implemented.

## Protocol Milestones

The revised protocol direction requires these milestones in order:

1. Define the minimum VaydeNet identity and the meaning of each common field.
2. Define serialization rules independently of packed C++ memory layouts.
3. Define message-type and capability identifiers.
4. Define the first nRF24L01 32-byte transport profile.
5. Implement and hardware-test single-frame nRF24L01 messages.
6. Add optional fragmentation only for messages that exceed the single-frame payload.
7. Revise the ESP-NOW experiment to use the same logical message contract through an ESP-NOW-specific frame encoding.
8. Extract validated experiments into reusable adapters.
9. Connect the common message API and selected adapter to VaydeEngine startup.

## Repository State

The active branch is `agent/esp32-node-bootstrap` at commit `09aec17` (`Add VaydeNet packet and ESP-NOW examples`).

`DEVELOPMENT_STATUS.md`, the node-bootstrap work, the portable node-settings structure, the application-side settings-loader scaffold, and the ESP32 platform component are currently uncommitted. Unrelated ESP-NOW ignore files, nRF24L01 files, WT32-ETH01 files, and generated artifacts are also present in the working tree. These areas must remain separated and explicitly staged; `git add -A` is unsafe for this branch.

## Current Capability Boundary

VaydeNet currently has an earlier 220-byte packet structure, ESP-NOW prototypes using that structure, local ESP32 node-bootstrap work, an initial portable `NodeSettings` structure, and a settings-loader contract that currently reports `NotConfigured`.

It does not yet have stored-settings loading, settings validation, the revised VaydeNet identity contract, transport-native frame profiles, capability discovery, a working nRF24L01 VaydeNet frame, routing, reliable delivery, reusable adapters, or a running VaydeEngine.
