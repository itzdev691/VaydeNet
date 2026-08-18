# WT32-ETH01 Ethernet broadcast smoke test

This isolated PlatformIO example initializes the WT32-ETH01 LAN8720 Ethernet
interface, obtains a DHCP lease, sends the canonical 220-byte VaydeNet `Packet`
inside a raw Ethernet II broadcast frame once per second, and hosts a local HTTP
status dashboard.

It does not modify VaydeEngine or define new VaydeNet packet semantics.
It does not probe application server ports yet.

## Project structure

```text
ethernet-smoke/
├── .clangd
├── .vscode/
│   └── settings.json
├── data/
│   ├── app.js
│   ├── index.html
│   └── styles.css
├── include/
│   ├── AppConfig.h
│   ├── EthernetNetwork.h
│   ├── VaydeBroadcaster.h
│   └── WebDashboard.h
├── src/
│   ├── EthernetNetwork.cpp
│   ├── VaydeBroadcaster.cpp
│   ├── WebDashboard.cpp
│   └── main.cpp
├── platformio.ini
└── README.md
```

- `main.cpp` owns only Arduino startup and periodic scheduling.
- `EthernetNetwork` owns LAN8720 events, DHCP state, MAC access, and network
  diagnostics.
- `VaydeBroadcaster` owns the VaydeNet packet, Ethernet frame construction,
  transmission, and transmit statistics.
- `WebDashboard` owns the HTTP routes, LittleFS asset serving, and live JSON
  status endpoint.
- `data/` contains the independent HTML, CSS, and JavaScript files packed into
  the LittleFS image.
- `AppConfig` owns example constants and timing values.
- `.clangd` and `.vscode/settings.json` make editor diagnostics use the Xtensa
  compilation database instead of parsing ESP32 code as macOS code.

## Frame layout

| Bytes | Field |
| ---: | --- |
| 0-5 | Destination MAC `FF:FF:FF:FF:FF:FF` |
| 6-11 | WT32 Ethernet source MAC |
| 12-13 | Experimental EtherType `0x88B5` |
| 14-233 | Canonical 220-byte VaydeNet `Packet` |

The Ethernet frame is 234 bytes before the hardware-added Ethernet FCS. No
VaydeNet fragmentation is required because the packet is below Ethernet's
1500-byte payload MTU.

The example derives `senderID` from the 48-bit Ethernet MAC, increments
`sequenceNumber`, and fills `payload` and `length`. The remaining packet fields,
including `crc`, remain zero because their canonical values and CRC algorithm
are not yet defined by VaydeNet.

The broadcast remains inside the local Layer-2 broadcast domain. A switch will
normally flood it to the other ports in the same VLAN, including the router's
port. A router will not route this custom EtherType to another IP network.

## Build

From this directory:

```sh
pio run
```

Build the LittleFS image containing `data/index.html`, `data/styles.css`, and
`data/app.js`:

```sh
pio run -t buildfs
```

## Dashboard port

The HTTP port is a compile-time setting in `platformio.ini`:

```ini
-D VAYDENET_DASHBOARD_PORT=8080
```

Replace `8080` with any available TCP port from `1` through `65535`, then
rebuild and upload the firmware. Ports below `1024` are valid on the ESP32 but
commonly correspond to standard services. The HTML, CSS, and JavaScript files
do not contain a hard-coded port; the browser uses the same host and port from
which the page was loaded.

This setting controls the board's local TCP listener. It does not configure
router port forwarding. The dashboard is plain HTTP without authentication and
should remain on a trusted LAN.

The board exposes these routes:

| Route | Response |
| --- | --- |
| `/` | Dashboard HTML |
| `/styles.css` | Dashboard stylesheet |
| `/app.js` | One-second status polling and page updates |
| `/api/status` | Live Ethernet and broadcast statistics as JSON |
| `/health` | Plain-text `ok` health response |

## Editor diagnostics

Generate the local compilation database after adding, removing, or renaming a
source file:

```sh
pio run -t compiledb
```

Open this `ethernet-smoke` directory as the editor workspace. The generated
`compile_commands.json` is machine-local and intentionally excluded from Git.

## Upload

Connect the Flipper Zero USB-UART Bridge to the separate six-pin programming
block:

- Flipper pin 13 TX to WT32 programming RXD
- Flipper pin 14 RX to WT32 programming TXD
- Common ground

Enter download mode by connecting IO0 to ground, pulsing EN, and keeping IO0
grounded during both uploads. Upload the firmware:

```sh
pio run -t upload \
  --upload-port /dev/cu.usbmodemYOUR_DEVICE
```

Upload the separate LittleFS image containing the webpage:

```sh
pio run -t uploadfs \
  --upload-port /dev/cu.usbmodemYOUR_DEVICE
```

Release IO0 from ground and pulse EN after both uploads to boot the program
normally.

## Monitor

```sh
pio device monitor \
  --port /dev/cu.usbmodemYOUR_DEVICE --baud 115200
```

The device reports link state, negotiated speed and duplex, DHCP information,
link transitions, transmit attempts, driver-accepted frames, rejected frames,
submitted byte count, the latest ESP-IDF transmit result, and the final
dashboard URL. Open the reported URL, for example:

```text
http://192.168.1.50:8080/
```

`ESP_OK` means the ESP32 Ethernet driver accepted the frame. It is not an
acknowledgement from the switch or router. Confirm reception with a capture on
the router or a mirrored switch port using the Wireshark filter:

```text
eth.type == 0x88b5
```
