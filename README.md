# VaydeNet

> **One communication interface. Any communication technology.**

VaydeNet is a hardware-independent communication framework that provides a single interface for applications to communicate across multiple networking technologies. Instead of writing separate code for Wi-Fi, LoRa, ESP-NOW, Bluetooth, or other transports, applications communicate through VaydeNet while the framework handles the underlying implementation.

## The Problem

> **Every communication technology works differently, forcing developers to write different code for each one.**

Modern embedded applications often become tightly coupled to a specific communication technology. Migrating to another radio or supporting multiple transports usually requires significant code changes.

## The Solution

VaydeNet provides a consistent communication layer between applications and communication technologies.

Applications communicate with VaydeNet, and VaydeNet determines how messages are delivered using the available transport.

```
Application
      │
      ▼
   VaydeNet
      │
      ▼
Wi-Fi • LoRa • ESP-NOW • Bluetooth • Ethernet • More
      │
      ▼
Destination Device
```

This separation allows applications to remain independent of the underlying communication technology.

## Packages and Downloads

| Package | Source | Purpose |
| --- | --- | --- |
| VaydeEngine | [Browse package](packages/VaydeEngine/) · [PlatformIO manifest](packages/VaydeEngine/library.json) | Portable packet and logical-message processing. |
| Transport adapters | [Browse adapters](packages/adapters/) | Transport implementations. |
| Platform services | [Browse platforms](packages/platforms/) | Hardware and platform integrations. |

[Download VaydeNet source ZIP](https://github.com/itzdev691/VaydeNet/archive/refs/heads/main.zip)
includes the packages and applications from the default branch. To download
another branch, select it on GitHub and use **Code → Download ZIP**.

These links provide source code, not prebuilt firmware. The PlatformIO manifest
is included with VaydeEngine; a manifest alone does not establish publication
to the PlatformIO Registry.

## Build the Node

Install [PlatformIO](https://platformio.org/), then clone and build:

```sh
git clone https://github.com/itzdev691/VaydeNet.git
cd VaydeNet
pio run --project-dir apps/node --environment espnow_esp32_doit
```

This builds for the DOIT ESP32 DevKit V1. See
[apps/node/platformio.ini](apps/node/platformio.ini) for other configured boards.
The node uses the repository's ESP-IDF components; the engine package alone
does not provide a complete node application.

## Examples and Documentation

- [ESP-NOW sender and receiver](apps/examples/esp-now/README.md).
- [Development status](DEVELOPMENT_STATUS.md): implemented capabilities,
  planned transports, and hardware evidence. The current node prototype uses ESP-NOW.
- [Contributing](CONTRIBUTING.md): development setup, validation, and pull requests.

## License

[Apache License 2.0](LICENSE).
