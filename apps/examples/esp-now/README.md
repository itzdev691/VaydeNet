# ESP-NOW packet prototypes

These PlatformIO projects keep the VaydeESP sender and receiver prototypes inside the VaydeNet repository but outside the main node, gateway, engine implementation, and reusable transport adapters.

## Structure

- `sender/` is a standalone PlatformIO project that sends one packed VaydeNet `Packet` each second.
- `receiver/` is a standalone PlatformIO project that prints received packet fields and renders them on the 3.5-inch ILI9488 display.
- `../../../packages/VaydeEngine/include/VaydeNet/packet/Packet.h` is the single packet definition used by both prototypes.

Both prototypes use the Arduino framework through PlatformIO. They exercise the VaydeNet packet over ESP-NOW but are not the reusable ESP-NOW adapter.

## Radio configuration

- ESP-NOW channel: `1`
- Sender destination: broadcast (`FF:FF:FF:FF:FF:FF`) by default
- Serial baud rate: `115200`
- Packet wire size: `220` bytes

Replace `peerAddress` in the sender with the MAC address printed by the receiver to use unicast.

## Receiver display wiring

| Signal | GPIO |
| --- | ---: |
| CS | 10 |
| DC | 9 |
| RST | 8 |
| SCK | 12 |
| MOSI | 11 |

The receiver's PlatformIO project installs `GFX Library for Arduino` automatically.

## Build

Run these commands from the VaydeNet repository root:

```sh
export PLATFORMIO_CORE_DIR=/Volumes/ExtVault/.platformio-vaydenet
pio run -d apps/examples/esp-now/sender
pio run -d apps/examples/esp-now/receiver
```

## Flash

Connect one ESP32-S3 at a time, then run:

```sh
export PLATFORMIO_CORE_DIR=/Volumes/ExtVault/.platformio-vaydenet
pio run -d apps/examples/esp-now/sender --target upload
pio run -d apps/examples/esp-now/receiver --target upload
```

Open the native USB serial monitor at 115200 baud:

```sh
export PLATFORMIO_CORE_DIR=/Volumes/ExtVault/.platformio-vaydenet
pio device monitor -d apps/examples/esp-now/sender
pio device monitor -d apps/examples/esp-now/receiver
```

## Current protocol boundary

The example preserves the implemented packet fields and their packed byte layout. CRC calculation, packet-type values, flag meanings, TTL processing, length validation, acknowledgements, and duplicate suppression remain protocol work for VaydeEngine.
