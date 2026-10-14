#!/usr/bin/env python3
# Usage : python3 tests/slow_client.py <port> <password>
import socket, time, sys
P=int(sys.argv[1]); PW=sys.argv[2]
slow=socket.socket(); slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4096); slow.connect(("127.0.0.1",P))
slow.sendall(f"PASS {PW}\r\nNICK slow\r\nUSER slow 0 * :r\r\nJOIN #load\r\n".encode())
fast=socket.create_connection(("127.0.0.1",P)); fast.sendall(f"PASS {PW}\r\nNICK fast\r\nUSER fast 0 * :r\r\nJOIN #load\r\n".encode())
fast.settimeout(0.05); time.sleep(0.2); data=b""
for i in range(50000):     # ~21 Mo > MAX_OUTBUF (16 Mo)
    fast.sendall(("PRIVMSG #load :"+"x"*400+"\r\n").encode())
    if i%100==0:
        try: data+=fast.recv(65536)
        except socket.timeout: pass
time.sleep(1.0); fast.settimeout(0.5)
try:
    while True:
        d=fast.recv(65536)
        if not d: break
        data+=d
except socket.timeout: pass
print("slow QUIT seen by fast:", b":slow!slow@127.0.0.1 QUIT" in data)
