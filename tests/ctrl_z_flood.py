#!/usr/bin/env python3
# Usage : python3 tests/ctrl_z_flood.py <port> <nb_messages> <fichier_sortie>   (mot de passe du serveur : pw)
# Reproduit le test de la grille : nc suspendu (^Z) dans un canal, flood, puis reprise
import socket, subprocess, time, sys, os, signal
P, N = sys.argv[1], int(sys.argv[2])
out = open(sys.argv[3], "w")
nc = subprocess.Popen(["nc", "-C", "127.0.0.1", P], stdin=subprocess.PIPE, stdout=out)
nc.stdin.write(b"PASS pw\nNICK zz\nUSER zz 0 * :z\nJOIN #f\n"); nc.stdin.flush(); time.sleep(0.3)
os.kill(nc.pid, signal.SIGSTOP)
f = socket.create_connection(("127.0.0.1", int(P)))
f.sendall(b"PASS pw\r\nNICK flooder\r\nUSER f 0 * :f\r\nJOIN #f\r\n"); time.sleep(0.2)
for i in range(N):
    f.sendall(("PRIVMSG #f :msg %06d %s\r\n" % (i, "y"*80)).encode())
time.sleep(2)
os.kill(nc.pid, signal.SIGCONT)
nc.stdin.write(b"PRIVMSG #f :back\n"); nc.stdin.flush()
time.sleep(4)
f.settimeout(1); data=b""
try:
    while True:
        d=f.recv(65536)
        if not d: break
        data+=d
except socket.timeout: pass
nc.stdin.close(); nc.terminate(); out.close()
got = sum(1 for l in open(sys.argv[3], errors="replace") if "PRIVMSG #f :msg" in l)
print(f"N={N} recus par nc={got} | flooder voit 'back'={b'back' in data} | flooder voit QUIT zz={b'zz!' in data and b'QUIT' in data}")
