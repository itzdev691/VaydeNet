# nRF24 register checks

This one-shot ESP-IDF application constructs the actual adapter and runs its startup
checks. It never sends a packet; after successful checks the adapter raises CE and
leaves the radio listening. It is separate from the ESP-NOW node.

Test wiring is fixed in the adapter: CE=5, CSN=4, MOSI=1, MISO=6, SCK=7.
Supply the radio with 3.3 V and a common ground. Confirm these GPIOs are available
on the actual board before uploading; these profiles do not establish board wiring.

Build from the repository root:

```sh
pio run -d apps/examples/nrf24l01/register-check -e nrf24_check_c5
pio run -d apps/examples/nrf24l01/register-check -e nrf24_check_p4
pio run -d apps/examples/nrf24l01/register-check -e nrf24_check_p4_rev3
```

The P4 environments reuse the node revision defaults: `nrf24_check_p4` is for
pre-v3 silicon and `nrf24_check_p4_rev3` is for production revision 3.x. Select
the actual chip revision before flashing.

For a connected board, use the matching environment and explicit USB port:

```sh
pio run -d apps/examples/nrf24l01/register-check -e nrf24_check_c5 -t upload --upload-port PORT
pio device monitor --port PORT --baud 115200
```

A completed check logs `Register and FIFO checks passed; CE low, powered standby`,
then `Receive mode active; transport ready` and `Checks passed; radio left listening
on pipe 0`. SPI failures or mismatches log the failed register or startup state,
omit those lines and end with `Transport unavailable`. A successful build or mock
test does not establish physical register responses. Successful hardware checks do
not establish RF delivery.

The startup profile uses channel 76, 1 Mbps, 0 dBm, two-byte CRC, five-byte address
`D2 F0 F0 F0 F0`, pipe-0 auto-ack, 1000 us retry delay / 15 retries, static 32-byte
payloads and disabled dynamic/ACK payloads. It verifies writable settings, clears
IRQ flags, flushes TX/RX and requires STATUS `0x0E` and FIFO_STATUS `0x11`.
The 32-byte payload width carries one `Nrf24Frame`: a four-byte header (format
`0xA1`, transfer ID, index, count) and 28 bytes of the 220-byte Packet, so each
Packet is eight frames. See `packages/adapters/nrf24l01/Nrf24Frame.h`.

Register behavior and timing follow [Nordic nRF24L01 Product Specification v2.0](https://devzone.nordicsemi.com/cfs-file/__key/support-attachments/beef5d1b77644c448dabff31668f3a47-aad1a46f307945a7b0204fd969e86bdf/content.pdf),
sections 5.5, 6, 8 and 9. The adapter waits 120 ms for supply ramp/reset and 1500 us
for oscillator startup, then enters RX mode (`CONFIG` `0x0F`, CE high).
