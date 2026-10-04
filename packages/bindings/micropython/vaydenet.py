"""Single-node ESP-NOW adapter for the native _vaydenet engine.

Call from one MicroPython task. Radio completion is not peer delivery proof.
"""
import _vaydenet
import espnow
import network

_BROADCAST = b"\xff" * 6
_radio = None


def init(channel=1):
    global _radio
    if not 1 <= channel <= 14:
        raise ValueError("channel must be 1..14")
    station = network.WLAN(network.WLAN.IF_STA)
    if station.isconnected():
        raise OSError("VaydeNet standalone mode requires disconnected station Wi-Fi")
    deinit()
    station.active(True)
    station.config(channel=channel, pm=network.WLAN.PM_NONE)
    radio = espnow.ESPNow()
    try:
        radio.active(True)
        radio.add_peer(_BROADCAST)
        if not _vaydenet.init(station.config("mac")):
            raise OSError("engine startup failed")
    except Exception:
        radio.active(False)
        _vaydenet.stop()
        raise
    _radio = radio


def deinit():
    global _radio
    if _radio is not None:
        _radio.active(False)
        _radio = None
    _vaydenet.stop()


def _require_radio():
    if _radio is None:
        raise OSError("call vaydenet.init() first")


def send(payload, ttl=1, flags=0):
    """Broadcast bytes. Returns Sent or Failed from radio completion."""
    _require_radio()
    status = _vaydenet.send(payload, ttl, flags)
    if status != "Queued":
        raise OSError(status)
    try:
        packet = _vaydenet.take_tx()
        success = _radio.send(_BROADCAST, packet, True)
    except Exception:
        _vaydenet.complete_tx(False)
        _vaydenet.poll_tx()
        raise
    _vaydenet.complete_tx(success)
    return _vaydenet.poll_tx()


def recv():
    """Poll one radio frame; return a decoded message dict or None."""
    _require_radio()
    peer, packet = _radio.irecv(0)
    if peer is not None:
        if len(packet) != 220:
            return None
        if not _vaydenet.feed(packet):
            raise OSError("native receive queue full")
    status, validation, message = _vaydenet.recv()
    if status == "QueueEmpty":
        return None
    if status != "MessageDelivered":
        raise ValueError("%s (validation=%d)" % (status, validation))
    return message
