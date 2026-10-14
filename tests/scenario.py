#!/usr/bin/env python3
# Usage : python3 tests/scenario.py <port> <password>
import socket, time, sys
P=int(sys.argv[1]); PW=sys.argv[2]
def c():
    s=socket.create_connection(("127.0.0.1",P)); s.settimeout(0.3); return s
def tx(s,d): s.sendall(d.encode()); time.sleep(0.15)
def rx(s):
    out=b""
    try:
        while True:
            d=s.recv(65536)
            if not d: out+=b"<EOF>"; break
            out+=d
    except socket.timeout: pass
    except ConnectionResetError: out+=b"<RESET>"
    return out.decode(errors="replace")
def reg(n):
    s=c(); tx(s,f"CAP LS 302\r\nPASS {PW}\r\nNICK {n}\r\nUSER {n} 0 * :Real {n}\r\nCAP END\r\n"); rx(s); return s
def show(t,s): print(f"--- {t}\n{rx(s)}")

# errors before reg
s=c(); tx(s,"JOIN #a\r\nNICK\r\nNICK 1bad\r\nPASS\r\n"); show("prereg errors",s)
tx(s,"PASS wrong\r\n"); show("bad pass",s)
# no pass
s=c(); tx(s,"NICK x\r\nUSER x 0 * :x\r\n"); show("no pass",s)
# partial
s=c(); 
for p in ["PA","SS ",PW+"\r\nNI","CK part\r\nUSER u 0 * :r\r","\n"]: s.sendall(p.encode()); time.sleep(0.05)
time.sleep(0.2); show("partial registration",s); s.close()
a=reg("alice"); b=reg("bob"); d=reg("carol")
tx(b,"nick ALICE\r\n"); show("433 case",b)
tx(a,"JOIN #chan\r\n"); show("alice join",a)
tx(b,"JOIN #CHAN\r\n"); show("bob join",b); show("alice sees",a)
tx(a,"PRIVMSG #chan :hello world\r\n"); show("bob gets chan msg",b)
tx(b,"PRIVMSG alice :\x01DCC SEND f.txt 2130706433 5000 12\x01\r\n"); show("alice gets dcc",a)
tx(b,"TOPIC #chan :new topic\r\nMODE #chan +i\r\nKICK #chan alice\r\n"); show("bob not op",b)
tx(a,"MODE #chan +itkl secret 2\r\nMODE #chan\r\n"); show("alice modes",a); show("bob sees modes",b)
tx(d,"JOIN #chan\r\n"); show("carol +i",d)
tx(a,"INVITE carol #chan\r\n"); show("alice invite",a); show("carol invited",d)
tx(d,"JOIN #chan\r\n"); show("carol +k",d)
tx(d,"JOIN #chan secret\r\n"); show("carol +l",d)
tx(a,"MODE #chan -l+o bob\r\n"); show("alice -l +o",a)
tx(d,"JOIN #chan secret\r\n"); show("carol join ok",d)
tx(b,"TOPIC #chan :bob topic\r\nKICK #chan carol :bye\r\n"); show("bob op kick",b); show("carol kicked",d)
tx(a,"!roll 20\r\nPRIVMSG #chan :!roll 20\r\nPRIVMSG ircbot :time\r\nWHO #chan\r\nFOO\r\nMODE alice +i\r\nMODE alice\r\n"); show("bot/who/unk",a)
tx(b,"QUIT :gone\r\n"); show("bob quit",b); show("alice sees quit",a)
tx(a,"PING :tok\r\nPART #chan :bye\r\nPRIVMSG #chan :x\r\n"); show("ping part",a)
# long line
e=c(); e.sendall(b"A"*2000); time.sleep(0.2); show("no newline flood",e)
