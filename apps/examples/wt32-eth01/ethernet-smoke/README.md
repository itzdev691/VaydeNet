# WT32-ETH01 Ethernet telemetry and ESP-NOW sender

This isolated PlatformIO example uses the WT32-ETH01 LAN8720 interface for
Ethernet status and the local HTTP dashboard. It sends the canonical 220-byte
VaydeNet `Packet` directly over ESP-NOW once per second for the existing
VaydeESP receiver.

The receiver packet layout is unchanged. The previous raw Ethernet II
broadcast path and experimental EtherType `0x88B5` are no longer used.

## Data flow

```text
LAN8720 Ethernet status
        |
        +--> DHCP gateway TCP probes
        |          |
        |          +--> LittleFS dashboard and /api/status
        |          +--> compact packet telemetry
        |
        +--> Ethernet link telemetry
                  |
                  v
          220-byte VaydeNet Packet
                  |
                  v
          ESP-NOW broadcast on channel 1
                  |
                  v
          VaydeESP receiver display
```

The current telemetry payload reports the WT32 Ethernet link state, DHCP state,
IPv4 address, gateway, negotiated speed, duplex mode, and the latest gateway
TCP-probe results. It does not query router-internal statistics such as WAN
usage, client lists, CPU load, or temperature.

## Project structure

```text
ethernet-smoke/
├── data/
│   ├── app.js
│   ├── index.html
│   └── styles.css
├── include/
│   ├── AppConfig.h
│   ├── EthernetNetwork.h
│   ├── PortMonitor.h
│   ├── VaydeBroadcaster.h
│   └── WebDashboard.h
├── src/
│   ├── EthernetNetwork.cpp
│   ├── PortMonitor.cpp
│   ├── VaydeBroadcaster.cpp
│   ├── WebDashboard.cpp
│   └── main.cpp
├── platformio.ini
└── README.md
```

- `EthernetNetwork` owns LAN8720 link, DHCP, addressing, and link diagnostics.
- `PortMonitor` probes one configured TCP port every two seconds against the
  DHCP gateway and retains the latest result for each port.
- `VaydeBroadcaster` owns Wi-Fi station mode, ESP-NOW channel and peer setup,
  packet construction, transmission, and sender-side statistics.
- `WebDashboard` serves the LittleFS assets and live Ethernet, TCP-probe, and
  ESP-NOW status.
- `AppConfig` owns the dashboard port, ESP-NOW channel, probe ports, and timing
  constants.

## TCP port probes

The default probe target is the DHCP gateway. The configured ports are `8080`,
`42691`, `9443`, and `8081`; edit `kProbePorts` in `include/AppConfig.h` to
change them. The firmware probes one port every two seconds with a 250 ms
connection timeout, so a complete four-port sweep takes about eight seconds.

These are TCP connection probes, not ICMP pings. `Open` means the TCP handshake
succeeded. `Unavailable` combines connection refusal, timeout, and routing
failure because a basic connection probe cannot distinguish those causes.

Packet payloads use compact port states: `O` is open, `C` is
closed/unreachable, and `?` means the first probe has not completed.

## ESP-NOW contract

The sender broadcasts exactly `sizeof(Packet)`, which is statically checked as
220 bytes. The payload is not wrapped in a 14-byte Ethernet header.

Both devices must use:

- The same packed VaydeNet `Packet` field order and widths.
- ESP-NOW channel `1`.
- An unencrypted ESP-NOW broadcast peer at `FF:FF:FF:FF:FF:FF`.

The packet's `senderID` is derived from the WT32 Wi-Fi station MAC and its
`sequenceNumber` increments before each send. `length` contains the number of
telemetry bytes including the terminating null byte, matching the working
VaydeESP sender. Version, type, flags, TTL, and CRC remain zero until VaydeNet
defines their canonical values.

The ESP-NOW queue result and send callback are sender-side evidence. The
receiver's serial output or display is required to prove end-to-end reception.

## Build

From this directory:

```sh
pio run
```

Build the LittleFS image:

```sh
pio run -t buildfs
```

Generate the editor compilation database:

```sh
pio run -t compiledb
```

## Dashboard

The committed `platformio.ini` sets the local dashboard listener to TCP port
`19691`. Change `VAYDENET_DASHBOARD_PORT` there and rebuild to use another port.

Routes:

| Route | Response |
| --- | --- |
| `/` | Dashboard HTML |
| `/styles.css` | Dashboard stylesheet |
| `/app.js` | One-second status polling and page updates |
| `/api/status` | Live Ethernet, TCP-probe, and ESP-NOW statistics as JSON |
| `/health` | Plain-text `ok` response |

LittleFS dashboard assets are uploaded separately from firmware.

## Upload

Connect the Flipper Zero USB-UART Bridge to the WT32 programming block:

- Flipper pin 13 TX to WT32 programming RXD.
- Flipper pin 14 RX to WT32 programming TXD.
- Common ground.

Enter download mode by grounding IO0, pulsing EN, and retaining IO0 at ground
during upload.

```sh
pio run -t upload --upload-port /dev/cu.usbmodemYOUR_DEVICE
pio run -t uploadfs --upload-port /dev/cu.usbmodemYOUR_DEVICE
```

Release IO0 and pulse EN to boot normally.

## Monitor

```sh
pio device monitor \
  --port /dev/cu.usbmodemYOUR_DEVICE --baud 115200
```

The serial log reports Ethernet state, gateway TCP probes, ESP-NOW
initialization, station MAC, channel, queued packets, queue failures,
send-callback results, and the dashboard URL.
