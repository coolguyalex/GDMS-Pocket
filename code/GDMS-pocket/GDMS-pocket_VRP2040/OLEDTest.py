import time
import board
import busio
import adafruit_ssd1306

# --- I2C setup ---
i2c = board.I2C()  # uses board.SCL and board.SDA

# Wait for I2C bus
while not i2c.try_lock():
    pass
devices = i2c.scan()
i2c.unlock()

print("I2C devices found:", [hex(d) for d in devices])

if not devices:
    raise RuntimeError("No I2C devices found. Check SDA/SCL, VCC=3V, GND common.")

# Most SSD1306 are 0x3C, sometimes 0x3D
addr = 0x3C if 0x3C in devices else devices[0]
print("Using OLED address:", hex(addr))

# --- OLED setup ---
WIDTH = 128
HEIGHT = 64  # most 0.96" are 128x64; if yours is 128x32 change to 32
oled = adafruit_ssd1306.SSD1306_I2C(WIDTH, HEIGHT, i2c, addr=addr)

# --- Draw test ---
oled.fill(0)
oled.text("OLED OK", 0, 0, 1)
oled.text(f"addr {hex(addr)}", 0, 10, 1)
oled.text("GDMS-pocket", 0, 20, 1)
oled.show()

# Simple animation so you know it’s alive
x = 0
dx = 2
while True:
    oled.fill(0)
    oled.text("OLED OK", 0, 0, 1)
    oled.fill_rect(x, 40, 20, 8, 1)
    oled.show()
    x += dx
    if x <= 0 or x >= (WIDTH - 20):
        dx = -dx
    time.sleep(0.05)
