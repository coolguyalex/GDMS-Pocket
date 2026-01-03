import time
import board
import digitalio

# Button pin mapping
buttons = {
    "UP": board.D5,
    "DOWN": board.D6,
    "A": board.D9,
    "B": board.D10,
}

btn = {}

for name, pin in buttons.items():
    b = digitalio.DigitalInOut(pin)
    b.direction = digitalio.Direction.INPUT
    b.pull = digitalio.Pull.UP   # active LOW
    btn[name] = b

print("Button test started")
print("Press buttons...")

prev = {name: True for name in btn}

while True:
    for name, b in btn.items():
        current = b.value  # True = not pressed, False = pressed
        if prev[name] and not current:
            print(f"{name} pressed")
        prev[name] = current
    time.sleep(0.05)
