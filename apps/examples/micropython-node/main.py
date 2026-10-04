import time
import sys
from machine import Pin, SoftI2C
import vaydenet

# Driver files may live in /display or the normal /lib import directory.
if "/display" not in sys.path:
    sys.path.append("/display")

# Assumes a 128x64 OLED. Pin numbers are GPIO numbers.
OLED_CONTROLLER = "SH1106"  # Diagnostic choice; match the actual controller.
OLED_WIDTH = 128
OLED_HEIGHT = 64
OLED_ROTATION = 180
OLED_MARGIN = 4
OLED_ADDRESS = 0x3C
OLED_SDA = 2
OLED_SCL = 3
RECEIVE_GPIO = 24
RECEIVE_PULSE_MS = 500

if OLED_CONTROLLER == "SH1106":
    from sh1106 import SH1106_I2C as OLED_I2C
elif OLED_CONTROLLER == "SSD1306":
    from ssd1306 import SSD1306_I2C as OLED_I2C
else:
    raise ValueError("OLED_CONTROLLER must be SH1106 or SSD1306")

receive_pin = Pin(RECEIVE_GPIO, Pin.OUT, value=0)
i2c = SoftI2C(sda=Pin(OLED_SDA), scl=Pin(OLED_SCL), freq=400000)
if OLED_ROTATION not in (0, 180):
    raise ValueError("OLED_ROTATION must be 0 or 180")
if OLED_CONTROLLER == "SH1106":
    oled = OLED_I2C(OLED_WIDTH, OLED_HEIGHT, i2c, addr=OLED_ADDRESS,
                    rotate=OLED_ROTATION)
else:
    oled = OLED_I2C(OLED_WIDTH, OLED_HEIGHT, i2c, addr=OLED_ADDRESS)
    # SSD1306.rotate uses a boolean mapping, unlike SH1106's degrees.
    oled.rotate(0 if OLED_ROTATION == 180 else 1)
oled.invert(False)  # Lit text on a dark background.
log_columns = (OLED_WIDTH - 2 * OLED_MARGIN) // 8
log_rows = (OLED_HEIGHT - 2 * OLED_MARGIN) // 8
log_lines = []


def log(*values):
    """Wrap each log entry and keep only the visible rows."""
    text = " ".join(str(value) for value in values)
    for line in text.split("\n"):
        for start in range(0, max(1, len(line)), log_columns):
            log_lines.append(line[start:start + log_columns])
            if len(log_lines) > log_rows:
                log_lines.pop(0)
    oled.fill(0)
    for row, line in enumerate(log_lines):
        oled.text(line, OLED_MARGIN, OLED_MARGIN + row * 8)
    oled.show()


log("VaydeNet init")
vaydenet.init(channel=1)
log("VaydeNet ready")
last_send = time.ticks_add(time.ticks_ms(), -5000)
receive_low_at = None
while True:
    now = time.ticks_ms()
    if time.ticks_diff(now, last_send) >= 5000:
        last_send = now
        try:
            status = vaydenet.send(b"hello from MVAYDE")
        except OSError as error:
            log("heartbeat error:", error)
        else:
            log("heartbeat:", status)
    try:
        message = vaydenet.recv()
    except ValueError as error:
        log("rejected:", error)
        message = None
    if message is not None:
        # Each validated receive restarts the nonblocking HIGH pulse.
        receive_pin.value(1)
        receive_low_at = time.ticks_add(time.ticks_ms(), RECEIVE_PULSE_MS)
        log("received:", message)
    if receive_low_at is not None and time.ticks_diff(time.ticks_ms(), receive_low_at) >= 0:
        receive_pin.value(0)
        receive_low_at = None
    time.sleep_ms(10)
