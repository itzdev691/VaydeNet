"""CPython simulation of heartbeat failure, receive polling and GPIO timeout."""
import pathlib
import runpy
import sys
import types


class StopSimulation(BaseException):
    pass


class Clock:
    now = 0

    @classmethod
    def sleep_ms(cls, interval):
        cls.now += interval
        if cls.now >= 5100:
            raise StopSimulation


class Pin:
    OUT = 1
    transitions = []

    def __init__(self, number, mode=None, value=None):
        self.number = number
        if value is not None:
            self.value(value)

    def value(self, level):
        self.transitions.append((self.number, Clock.now, level))


class Display:
    def __init__(self, *args, **kwargs):
        pass

    def invert(self, value):
        pass

    def fill(self, value):
        pass

    def text(self, *args):
        pass

    def show(self):
        pass


class Radio:
    sends = 0
    receives = 0

    @staticmethod
    def init(channel):
        assert channel == 1

    @classmethod
    def send(cls, payload):
        cls.sends += 1
        assert payload == b"hello from MVAYDE"
        if cls.sends == 1:
            raise OSError("transient send failure")
        return "Sent"

    @classmethod
    def recv(cls):
        cls.receives += 1
        if cls.receives == 1:
            return {"payload": b"valid receive"}
        return None


machine = types.ModuleType("machine")
machine.Pin = Pin
machine.SoftI2C = lambda **kwargs: None
display = types.ModuleType("sh1106")
display.SH1106_I2C = Display
clock = types.ModuleType("time")
clock.ticks_ms = lambda: Clock.now
clock.ticks_add = lambda now, delta: now + delta
clock.ticks_diff = lambda now, previous: now - previous
clock.sleep_ms = Clock.sleep_ms
sys.modules.update({"machine": machine, "sh1106": display,
                    "vaydenet": Radio, "time": clock})

example = pathlib.Path(__file__).resolve().parents[2] / "apps/examples/micropython-node/main.py"
try:
    runpy.run_path(str(example))
except StopSimulation:
    pass

assert Radio.sends == 2, "heartbeat did not retry on its five-second schedule"
assert Radio.receives == 510, "receive polling stopped after the send error"
assert Pin.transitions == [(24, 0, 0), (24, 0, 1), (24, 500, 0)]
print("PASS: heartbeat error recovery, continued receive polling and GPIO pulse cleanup")
