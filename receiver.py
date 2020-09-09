import socket
import time
from pynput.mouse import Button, Controller
import sys
from array import array

HOST = '0.0.0.0' # any address
PORT = 5002
f = 1/80
mouse = Controller()
print(f"{HOST}, {PORT}")
while True:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        s.settimeout(10)
        try:
            print("Accepting connection...")
            conn, addr = s.accept()
            with conn:
                print('Connected by ', addr)
                conn.settimeout(1.0)
                # conn.setblocking(False)
                while True:
                    conn.send(b'\n')
                    data = conn.recv(10)
                    if data:
                        data = array('b', data)
                        mouse.move(*(data[1:3]))
                        if data[0] & 0x04:
                            mouse.click(Button.left, 1)
                        if data[0] & 0x01:
                            mouse.click(Button.right, 1)
                        time.sleep(f)
        except:
            print("Connection failed, retry every second")
            time.sleep(1)