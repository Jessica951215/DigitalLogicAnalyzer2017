import sys

# configure the serial connections (the parameters differs on the device you are connecting to)
ser = serial.Serial(
    port='/dev/ttyACM0',
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
)

# Print the bytes feched
counter = 0
f = open('myproject/data.txt', 'w')
while True:
    ch=ser.read()
    a = str(bin(int.from_bytes(ch,byteorder='big')))[2:].zfill(8)
    counter += 1
    f.write(a)