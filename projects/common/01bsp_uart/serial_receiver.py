# Import the serial module
import serial# Define the serial port as a parameter

serial_port = input("Enter the serial port (COM* in Windows, /dev/ttyUSB* in Linux, /dev/tty.usb*): ")# Create a serial object with the given port and baud rate

ser = serial.Serial(serial_port, 115200)# Loop indefinitely

while True:

    # Read a line from the serial output
    line = ser.readline()

    # Decode the line as UTF-8 and strip any whitespace
    line = line.decode("utf-8").strip()

    # Print the line to the console
    print(line)