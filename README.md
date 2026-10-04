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

Download the complete repository from [Releases](https://github.com/itzdev691/VaydeNet/releases)
using a release's **Source code (zip)** archive, then extract it. The archive
includes the engine, adapters, platform services, and applications.

For the latest default-branch source, use
[Download VaydeNet source ZIP](https://github.com/itzdev691/VaydeNet/archive/refs/heads/main.zip).
To download another branch, select it on GitHub and use **Code → Download ZIP**.

These links provide source code, not prebuilt firmware. The PlatformIO manifest
is included with VaydeEngine; a manifest alone does not establish publication
to the PlatformIO Registry.

## Use the C++ Library

After extracting the repository, the reusable library is in
`packages/VaydeEngine/`. Copy that folder into your application and retain the
repository's [LICENSE](LICENSE) file with the copied source.

Configure your application's build to:

- use C++17 or later;
- add `VaydeEngine/include/` to its compiler include paths;
- compile `VaydeEngine/src/VaydeEngine.cpp`,
  `VaydeEngine/src/PacketValidation.cpp`,
  `VaydeEngine/src/PacketMessageDecoder.cpp`, and
  `VaydeEngine/src/PacketMessageEncoder.cpp` with the application.

These paths are relative to wherever you place the copied `VaydeEngine/`
folder. Application code can then include the engine's public header:

```cpp
#include <VaydeNet/VaydeEngine.h>
```

Your application supplies the transport and startup dependencies through the
engine's interfaces. Add the adapters and platform services needed for your
target separately. See the [engine package README](packages/VaydeEngine/README.md)
for its scope and package layout.

The engine is compiled as part of your application's build. Its current
`CMakeLists.txt` registers an ESP-IDF component; it is not a standalone CMake
project. Building `apps/node` is only required when you want the supplied node
firmware.

## Build the Node Firmware

Keep the complete extracted repository for this build. Install
[PlatformIO](https://platformio.org/), open a terminal in the extracted
repository folder (the folder containing this README), then run:

```sh
pio run --project-dir apps/node --environment espnow_esp32_doit
```

This builds the node firmware for the DOIT ESP32 DevKit V1. See
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
