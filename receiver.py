import socket
import time
from pynput.mouse import Button, Controller
import sys
from array import array

# HOST = '0.0.0.0' # any address
HOST = '192.168.1.1' # any address
PORT = 5002
f = 1/125
mouse = Controller()
print(f"{HOST}, {PORT}")

while True:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.settimeout(1)
        try:
            s.connect((HOST, PORT))
            print('Connected')
            s.settimeout(10)
            while True:
                s.send(b'\n')
                data = s.recv(10)
                if data:
                    data = array('b', data)
                    # print(data)
                    mouse.move(*(data[1:3]))
                    if data[0] & 0x04:
                        mouse.click(Button.left, 1)
                    if data[0] & 0x01:
                        mouse.click(Button.right, 1)
                    time.sleep(f)
        except:
            print("Connection failed, retry every second")
            time.sleep(1)