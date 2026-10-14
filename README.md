*This project has been created as part of the 42 curriculum by apesic.*

# ft_irc

## Description

`ft_irc` is an IRC server written in C++98. It follows the client-server part of
the IRC protocol (RFC 1459 / 2812), so a real IRC client such as irssi, WeeChat or
HexChat can connect to it and use it like an official server.

Overview:

- a single `poll()` loop handles every socket (listening, reading, writing), all
  file descriptors are non-blocking;
- partial packets are buffered and rebuilt into complete lines before being executed;
- outgoing data is queued per client and only sent when `poll()` reports the socket
  is writable, so a slow client never blocks the others;
- registration with `PASS`, `NICK`, `USER` (and `CAP` negotiation for real clients);
- channels with regular users and operators: `JOIN`, `PART`, `PRIVMSG`, `NOTICE`,
  `KICK`, `INVITE`, `TOPIC`, `MODE` (`i`, `t`, `k`, `o`, `l`), plus `NAMES`, `WHO`,
  `LIST`, `PING`, `QUIT`;
- bonus: a built-in bot (`ircbot`) and file transfer through DCC.

## Instructions

### Build

```sh
make        # builds ./ircserv
make bonus  # same binary: the bot is built in
make clean / make fclean / make re
```

Requires a C++ compiler; flags are `-Wall -Wextra -Werror -std=c++98`.

### Run

```sh
./ircserv <port> <password>
./ircserv 6667 secret
```

### Connect

With irssi (reference client):

```
/connect localhost 6667 secret mynick
/join #42
/msg ircbot help
```

With netcat (`-C` sends `\r\n`):

```sh
nc -C 127.0.0.1 6667
PASS secret
NICK alice
USER alice 0 * :Alice
JOIN #42
PRIVMSG #42 :hello
```

### Bonus

- **Bot**: send `!help`, `!time`, `!roll [max]`, `!users` or `!ping` in a channel, or
  a private message to `ircbot`.
- **File transfer**: DCC is client-to-client; the server relays the CTCP
  `DCC SEND` request untouched. In irssi: `/dcc send <nick> <file>`, then the
  receiver types `/dcc get <nick>`.

## Resources

- [RFC 1459 – Internet Relay Chat Protocol](https://www.rfc-editor.org/rfc/rfc1459)
- [RFC 2812 – IRC Client Protocol](https://www.rfc-editor.org/rfc/rfc2812)
- [Modern IRC documentation](https://modern.ircdocs.horse/) – numerics and message formats
- [IRCv3 capability negotiation](https://ircv3.net/specs/extensions/capability-negotiation)
- [DCC protocol](https://modern.ircdocs.horse/dcc)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- `man 2 poll`, `man 2 socket`, `man 2 recv`, `man 2 send`

### AI usage

AI (Claude) was used to:

- review the existing network code (socket setup, `poll()` loop, input buffer) and
  list its bugs (missing `POLLOUT` handling, wrong event mask, unbounded buffers,
  no exception handling);
- write the IRC protocol layer: line parser, command dispatcher, registration,
  channel commands, `MODE` handling, and the built-in bot;
- write a test script (netcat-like scenarios, slow-reader and load tests) and the
  French documentation in `doc/`.

All generated code was read, tested with netcat-like scripts and valgrind, and
is explained in `doc/`.
