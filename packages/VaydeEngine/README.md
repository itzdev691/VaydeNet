# VaydeEngine

VaydeEngine is the reusable, transport-agnostic core of VaydeNet.

It handles VaydeNet packet validation, message encoding and decoding, receive processing, and transmit requests without owning a specific radio or board implementation.

## Scope

This package contains the reusable engine only.

It does not include:

- complete flashable node firmware
- ESP-NOW or other transport adapters
- ESP32-specific platform support
- application code

Those remain separate in the VaydeNet repository so applications can combine the engine with only the transports and platform support they need.

## PlatformIO

The package manifest is `library.json`.

When published to the PlatformIO Registry, projects can depend on VaydeEngine as a normal PlatformIO library package.

## Repository layout

```text
VaydeEngine/
├── library.json
├── CMakeLists.txt
├── include/
│   └── VaydeNet/
└── src/
```

## License

Apache-2.0.
