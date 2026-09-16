# Contributing to VaydeNet

VaydeNet is an early-stage embedded communication framework. Contributions
should keep the portable engine independent of any single radio, board, or
vendor SDK and should describe exactly what was tested.

Read [README.md](README.md) for the project overview and
[DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md) for the current implementation
boundary before starting work. Planned capabilities in those documents are not
necessarily implemented.

## Repository layout

- `packages/VaydeEngine/` contains portable framework code.
- `packages/adapters/` contains transport-specific adapters.
- `packages/platforms/` contains board and platform integrations.
- `apps/node/` contains the ESP32 node firmware.
- `apps/examples/` contains experiments and hardware examples. Examples are not
  automatically part of the production node path.
- `tests/` contains host-side tests and ESP-IDF/FreeRTOS test doubles.

Keep hardware APIs out of `VaydeEngine`. Put transport behavior in an adapter
and board- or SDK-specific behavior in a platform package.

## Prepare a change

1. Fork and clone the repository.
2. Create a focused branch from the latest `main`.
3. Make one coherent change. Avoid unrelated formatting, generated files, or
   build artifacts.
4. Add or update tests for changed behavior.
5. Update `DEVELOPMENT_STATUS.md` when the implemented capability, evidence, or
   known limitations change.

Use clear branch names such as `feature/esp-now-peer-management`,
`fix/packet-length-validation`, or `docs/contributing-guide`.

## Development requirements

Host tests require a C++17 compiler. They use `clang++` by default and enable
warnings as errors plus AddressSanitizer and UndefinedBehaviorSanitizer.

Firmware builds use [PlatformIO](https://platformio.org/). CI currently pins
PlatformIO Core `6.1.19` and Python `3.11`.

## Validate the change

Run the host test suite from the repository root:

```sh
bash tests/run_host_tests.sh
```

Build each affected node environment:

```sh
pio run --project-dir apps/node --environment espnow_esp32s3
pio run --project-dir apps/node --environment espnow_esp32s3_mini
pio run --project-dir apps/node --environment espnow_esp32c5
pio run --project-dir apps/node --environment esp32-s2
```

Run this whitespace check before committing:

```sh
git diff --check
```

The pull-request workflow reruns the host tests and all four node builds. A
successful build proves compilation and linking only. It does not prove that
firmware flashed, booted, exchanged radio frames, or completed a logical
VaydeNet workload.

For hardware changes, record each verified layer separately:

- exact board and PlatformIO environment;
- build result;
- flash result and device port;
- serial or runtime result;
- peer-device setup and observed behavior;
- limitations and untested paths.

Do not describe a sender-side queue result or callback as end-to-end delivery.
Receiver-side evidence is required. Remove secrets, personal data, and
irrelevant device identifiers from logs before sharing them.

## Code and protocol changes

- Use C++17 and follow the style of the surrounding code.
- Preserve portable interfaces between the engine, transports, and platforms.
- Return explicit status values for expected embedded failures.
- Keep callbacks nonblocking and make buffer ownership clear.
- Add deterministic host coverage where hardware behavior can be isolated.
- Treat the packed 220-byte `Packet` as a current prototype, not a finalized
  universal wire format.
- When changing packet fields or validation, update producers, consumers,
  tests, and CRC handling together.

`apps/node/dependencies.lock` is tracked but target-sensitive. A PlatformIO
build can rewrite it for the last selected target. Review any lockfile change
and include it only when it is intentional and relevant to the pull request.

## Commits and pull requests

Write concise, imperative commit subjects. Keep commits reviewable and do not
commit `.pio/`, local build output, editor state, credentials, or captured logs
containing private information.

A pull request should include:

- the problem and the bounded change;
- the affected packages, adapters, applications, or boards;
- exact commands run and their results;
- hardware evidence, when applicable;
- known limitations and follow-up work;
- documentation updates required by the change.

Keep the branch current with `main`, resolve review comments with focused
follow-up commits, and wait for required checks before merge.

## License

By submitting a contribution, you agree that it is licensed under the
[Apache License 2.0](LICENSE) used by this repository.
