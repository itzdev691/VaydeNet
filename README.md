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

## Features

- Hardware-independent communication
- Transport abstraction
- Modular architecture
- Decentralized operation
- Multi-hop routing support
- Peer discovery
- Reliable message delivery
- Extensible adapter system
- Designed for resource-constrained embedded devices

## Goals

- Provide one communication API across multiple technologies.
- Keep applications independent of transport implementation.
- Support a wide range of communication technologies.
- Make adding new transports straightforward.
- Build a lightweight framework suitable for embedded systems.

## Architecture

VaydeNet acts as the bridge between applications and communication technologies.

```
Application
      │
      ▼
 VaydeNet Core
      │
 ┌────┴──────────────┐
 │                   │
 ▼                   ▼
ESP-NOW Adapter   LoRa Adapter
 │                   │
 ▼                   ▼
ESP-NOW Radio     LoRa Radio
```

Additional adapters can be added without changing application code.

## Philosophy

Applications should describe **what** they want to communicate.

VaydeNet decides **how** the communication happens.

## Status

VaydeNet is currently under active development and the architecture is evolving.

## License

License information will be added when the project reaches its first public release.
