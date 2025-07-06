import RPi.GPIO as GPIO
import time

GPIO.setmode(GPIO.BCM)
RELAY_PIN = 17

GPIO.setup(RELAY_PIN, GPIO.OUT)

GPIO.output(RELAY_PIN, GPIO.HIGH)

time.sleep(10)

GPIO.output(RELAY_PIN, GPIO.LOW)

GPIO.cleanup()
