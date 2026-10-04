"""Actual native engine with a fake radio; no hardware/RF claims."""
import sys
import _vaydenet


class FakeStation:
    IF_STA = 0
    PM_NONE = 0
    connected = False

    def __init__(self, interface):
        assert interface == self.IF_STA

    def isconnected(self):
        return self.connected

    def active(self, value):
        pass

    def config(self, *args, **kwargs):
        if args:
            assert args == ("mac",)
            return b"\x01\x02\x03\x04\x05\x06"
        assert kwargs == {"channel": 1, "pm": self.PM_NONE}


class FakeRadio:
    latest = None

    def __init__(self):
        FakeRadio.latest = self
        self.incoming = []
        self.fail = False
        self.raise_send = False

    def active(self, value):
        pass

    def add_peer(self, peer):
        assert peer == b"\xff" * 6

    def send(self, peer, packet, sync):
        assert peer == b"\xff" * 6 and sync
        if self.raise_send:
            raise OSError("fake radio failure")
        if self.fail:
            return False
        self.incoming.append((peer, packet))
        return True

    def irecv(self, timeout):
        assert timeout == 0
        return self.incoming.pop(0) if self.incoming else (None, None)


class NetworkModule:
    WLAN = FakeStation


class RadioModule:
    ESPNow = FakeRadio


sys.modules["network"] = NetworkModule()
sys.modules["espnow"] = RadioModule()
sys.path.insert(0, sys.argv[1])
import vaydenet

vaydenet.init()
assert vaydenet.recv() is None
assert vaydenet.send(b"\x00hello\xff") == "Sent"
message = vaydenet.recv()
assert message["payload"] == b"\x00hello\xff"
assert message["source"] == 0x010203040506
assert message["sequence"] == 0
radio = FakeRadio.latest
radio.fail = True
assert vaydenet.send(b"failed") == "Failed"
radio.fail = False
radio.raise_send = True
try:
    vaydenet.send(b"exception")
    assert False
except OSError:
    pass
radio.raise_send = False
assert vaydenet.send(b"after failure") == "Sent"
assert vaydenet.recv()["payload"] == b"after failure"
assert vaydenet.send(b"bad crc") == "Sent"
peer, packet = radio.incoming.pop()
corrupt = bytearray(packet)
corrupt[18] ^= 1
radio.incoming.append((peer, corrupt))
try:
    vaydenet.recv()
    assert False
except ValueError:
    pass
radio.incoming.append((peer, b"foreign frame"))
assert vaydenet.recv() is None
assert vaydenet.send(b"still running") == "Sent"
assert vaydenet.recv()["payload"] == b"still running"
FakeStation.connected = True
try:
    vaydenet.init()
    assert False
except OSError:
    pass
FakeStation.connected = False
vaydenet.deinit()
assert _vaydenet.send(b"stopped") == "NotStarted"
print("PASS: Python radio adapter with real native engine and fake radio")
