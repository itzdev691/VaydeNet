"""Run with a MicroPython unix build containing the _vaydenet user module."""
import _vaydenet as engine

engine.stop()
assert engine.send(b"hello") == "NotStarted"
assert engine.poll_tx() == "NotStarted"
mac = b"\x01\x02\x03\x04\x05\x06"
assert engine.init(mac)
assert engine.recv()[0] == "QueueEmpty"
payload = b"\x00hello\xff"
assert engine.send(payload, 2, 3) == "Queued"
assert engine.send(b"busy") == "Busy"
assert engine.poll_tx() == "Pending"
assert not engine.complete_tx(True)  # Transport has not taken the packet yet.
packet = engine.take_tx()
assert len(packet) == 220
assert engine.take_tx() is None
assert engine.complete_tx(True)
assert not engine.complete_tx(True)
assert engine.poll_tx() == "Sent"
assert engine.poll_tx() == "Empty"
assert engine.feed(packet)
status, validation, message = engine.recv()
assert status == "MessageDelivered"
assert message["payload"] == payload
assert message["source"] == 0x010203040506
assert message["sequence"] == 0
assert message["ttl"] == 2 and message["flags"] == 3
assert engine.send(b"next") == "Queued"
next_packet = engine.take_tx()
assert engine.complete_tx(False)
assert engine.poll_tx() == "Failed"
assert engine.feed(next_packet)
assert engine.recv()[2]["sequence"] == 1
corrupt = bytearray(packet)
corrupt[18] ^= 1
assert engine.feed(corrupt)
assert engine.recv()[0] == "PacketRejected"
for _ in range(4):
    assert engine.feed(packet)
assert not engine.feed(packet)
for _ in range(4):
    assert engine.recv()[0] == "MessageDelivered"
assert engine.recv()[0] == "QueueEmpty"
assert engine.send(b"x" * 201) == "InvalidMessageLength"
assert engine.send(b"x" * 200) == "Queued"
assert len(engine.take_tx()) == 220
assert engine.complete_tx(True)
assert engine.poll_tx() == "Sent"
for func, arg in ((engine.init, b"short"), (engine.feed, b"short")):
    try:
        func(arg)
        assert False
    except ValueError:
        pass
try:
    engine.send(b"x", 0)
    assert False
except ValueError:
    pass
assert engine.init(mac)  # Also resets engine state after a MicroPython soft reset.
assert engine.send(b"reset") == "Queued"
assert engine.feed(engine.take_tx())
assert engine.recv()[2]["sequence"] == 0
engine.stop()
assert not engine.feed(packet)
assert engine.recv()[0] == "NotStarted"
print("PASS: native MicroPython engine bindings, binary round trip, CRC, queue and TX state")
