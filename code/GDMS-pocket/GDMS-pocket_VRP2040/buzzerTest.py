import time
import board
import pwmio

buzzer = pwmio.PWMOut(board.D12, duty_cycle=0, frequency=440, variable_frequency=True)

def tone(freq, ms, duty=32768):
    buzzer.frequency = freq
    buzzer.duty_cycle = duty
    time.sleep(ms / 1000)
    buzzer.duty_cycle = 0
    time.sleep(0.05)

print("Buzzer test on D12")

# Simple beep pattern
for _ in range(3):
    tone(880, 120)
    time.sleep(0.1)

# Little scale
for f in [262, 330, 392, 523, 659, 784, 1047]:
    tone(f, 120)

while True:
    # heartbeat beep every 2 seconds
    tone(660, 80)
    time.sleep(2)
