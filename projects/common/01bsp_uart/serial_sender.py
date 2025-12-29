# Import the serial module
import serial# Define the serial port as a parameter
serial_port = input("Enter the serial port: ")# Create a serial object with the given port and baud rate
### for me: ls -la /dev/ttyACM*    
# crw-rw-rw- 1 root dialout 166, 0 10月 17 11:41 /dev/ttyACM0 
# crw-rw-rw- 1 root dialout 166, 1 10月 17 11:41 /dev/ttyACM1
# conda activate openwsn-fw    
# need to use different devices for sender and receiver,rather than the same device

ser = serial.Serial(serial_port, 115200)# Loop indefinitely

while True:

		# Prompt the user for a message to send
		message = input("Enter a message to send: ")

		# Encode the message as UTF-8 and add a return character at the end
		message = message.encode("utf-8") + b"\r\n"
		
		# Write the message to the serial output
		ser.write(message)