import time
import board
import digitalio

led = digitalio.DigitalInOut(board.D11)
led.direction = digitalio.Direction.OUTPUT

print("LED blink test started")

while True:
    led.value = True
    print("LED ON")
    time.sleep(0.5)

    led.value = False
    print("LED OFF")
    time.sleep(0.5)
