"""Run with the native MicroPython unix build, in a fresh interpreter."""
import _vaydenet as engine
import micropython

assert engine.init(b"123456")
send_status = None
busy_status = None
pending_status = None
micropython.heap_lock()
try:
    # No result strings naming these statuses appear in this script, so they
    # cannot accidentally intern the old binding's dynamically allocated result.
    send_status = engine.send(b"heap locked")
    busy_status = engine.send(b"second")
    pending_status = engine.poll_tx()
finally:
    micropython.heap_unlock()

assert send_status is not None and busy_status is not None
assert pending_status is not None
packet = engine.take_tx()
assert len(packet) == 220
assert engine.complete_tx(True)
assert engine.poll_tx() == "Sent"
engine.send(b"after completion")
assert len(engine.take_tx()) == 220
assert engine.complete_tx(False)
assert engine.poll_tx() == "Failed"
engine.stop()
print("PASS: native send statuses under heap lock and subsequent TX recovery")
