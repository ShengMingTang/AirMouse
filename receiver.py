import socket

HOST = '0.0.0.0' # any address
PORT = 5002
print(f"{HOST}, {PORT}")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()
    conn, addr = s.accept()
    with conn:
        print('Connected by ', addr)
        while True:
            data = conn.recv(1024)
            if not data:
                break
            print('str:', data)
            print('b  :', data[0], data[1], data[2])
            print()