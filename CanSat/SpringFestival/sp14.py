import RPi.GPIO as GPIO
import time
import sys

AIN1 = 13
AIN2 = 19
BIN1 = 6
BIN2 = 5

frequency = 50

GPIO.setmode(GPIO.BCM)
GPIO.setup(AIN1, GPIO.OUT)
GPIO.setup(AIN2, GPIO.OUT)
GPIO.setup(BIN1, GPIO.OUT)
GPIO.setup(BIN2, GPIO.OUT)

# PWM設定
pwm_A1 = GPIO.PWM(AIN1, frequency)
pwm_A2 = GPIO.PWM(AIN2, frequency)
pwm_B1 = GPIO.PWM(BIN1, frequency)
pwm_B2 = GPIO.PWM(BIN2, frequency)

pwm_A1.start(0)
pwm_A2.start(0)
pwm_B1.start(0)
pwm_B2.start(0)

speed = 60  # ←ここで調整（おすすめ50〜70）

def stop():
    pwm_A1.ChangeDutyCycle(0)
    pwm_A2.ChangeDutyCycle(0)
    pwm_B1.ChangeDutyCycle(0)
    pwm_B2.ChangeDutyCycle(0)

def forward():
    pwm_A1.ChangeDutyCycle(speed)
    pwm_A2.ChangeDutyCycle(0)
    pwm_B1.ChangeDutyCycle(speed)
    pwm_B2.ChangeDutyCycle(0)

def backward():
    pwm_A1.ChangeDutyCycle(0)
    pwm_A2.ChangeDutyCycle(speed)
    pwm_B1.ChangeDutyCycle(0)
    pwm_B2.ChangeDutyCycle(speed)

def left():
    pwm_A1.ChangeDutyCycle(0)
    pwm_A2.ChangeDutyCycle(speed)
    pwm_B1.ChangeDutyCycle(speed)
    pwm_B2.ChangeDutyCycle(0)

def right():
    pwm_A1.ChangeDutyCycle(speed)
    pwm_A2.ChangeDutyCycle(0)
    pwm_B1.ChangeDutyCycle(0)
    pwm_B2.ChangeDutyCycle(speed)

def get_key():
    try:
        import termios, tty
        fd = sys.stdin.fileno()
        old = termios.tcgetattr(fd)
        try:
            tty.setraw(fd)
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old)
        return ch.lower()
    except:
        return input().strip().lower()

try:
    print("W/A/S/Dで操作、spaceで停止、qで終了")

    while True:
        cmd = get_key()

        if cmd == 'w':
            forward()
        elif cmd == 's':
            backward()
        elif cmd == 'a':
            left()
        elif cmd == 'd':
            right()
        elif cmd == ' ':
            stop()
        elif cmd == 'q':
            break

        time.sleep(0.05)

except KeyboardInterrupt:
    pass

finally:
    stop()
    GPIO.cleanup()
