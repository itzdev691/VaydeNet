# Shared node runtime

ESP-IDF component for ESP32 node startup, settings, engine ownership, and
transport wiring. Applications include `VaydeNet/node/NodeBootstrap.h` and
provide a `MessageSink` that outlives the runtime.

```cpp
static ApplicationMessageSink sink;
static NodeBootstrap node(sink);
// Call node.run() and handle its startup status before processing traffic.
```

The application schedules `processNextPacket()`, submits logical messages through
`tryTransmit()`, and polls `pollTransmitCompletion()` for its outstanding send.
Keep these calls serialized in one task. This component does not create a worker
task or make the engine safe for concurrent callers. Application services must
avoid blocking that task for long periods.

## Ownership

- `packages/node`: bootstrap, NVS settings loading, concrete ESP32 dependencies.
- `packages/VaydeEngine`: portable validation, decoding, encoding, and protocol state.
- `packages/adapters/esp-now`: radio callbacks, receive queue, and send completion.
- `apps/node`: heartbeat scheduling, logging, and application message sink.
- `apps/wt32-eth01/ethernet-smoke`: Ethernet monitoring, probes, and dashboard.

Register this directory and its three dependencies in the application's
`EXTRA_COMPONENT_DIRS`, then add `node` to the application's `REQUIRES`.
Existing board model and activity LED build definitions are still required.

The WT32 application remains an Arduino application using its existing broadcaster.
Its integration with this ESP-IDF component requires a separate service port;
this extraction does not establish WT32 runtime or peer-delivery compatibility.
