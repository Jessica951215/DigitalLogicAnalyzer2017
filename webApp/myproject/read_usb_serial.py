import sys
import serial



# configure the serial connections (the parameters differs on the device you are connecting to)
ser = serial.Serial(
    port='/dev/ttyACM0',
    baudrate=115200,
    parity=serial.PARITY_NONE,
    stopbits=serial.STOPBITS_ONE,
    bytesize=serial.EIGHTBITS,
)

# Print the bytes feched
endStr = 'no more data || no more data || no more data'
read_chars = ' '*len(endStr)
counter = 0
ser.write(sys.argv[1].encode())
print(sys.argv[1])
readStr = b''
f = open('test2.txt','w')
writestr = ""
while read_chars != endStr:
	was_Read = (ser.read())
	read_chars = read_chars[1:] + was_Read.decode('utf8',errors='replace')	
	readStr += was_Read
	a = str(bin(int.from_bytes(was_Read,byteorder='big')))[2:].zfill(8)
	writestr += a
	f.write(a)
	counter += 1
	if counter % 2 == 0:
		f.write("\n")
f.close()
writestr = writestr[:len(writestr)-8*len(endStr)]
with open('test.txt','w') as test_output:
	test_output.write(writestr)

ser.close()