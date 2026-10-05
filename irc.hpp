#ifndef IRC_HPP
#define IRC_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>

#define SERVER_NAME "ircserv"
#define VERSION     "1.0"
#define MAX_LINE    512     // taille max d'un message IRC, \r\n compris
#define MAX_OUTBUF  16777216    // 16 Mo : un client suspendu (^Z) doit tout recevoir à son réveil
#define QUIT_DELAY  2       // secondes laissées à un client sortant pour recevoir ses derniers messages
#define NICK_LEN    30
#define CHAN_LEN    50
#define TOPIC_LEN   307
#define BOT_NICK    "ircbot"

std::string                 ircLower(const std::string& s);
std::string                 toUpper(const std::string& s);
std::vector<std::string>    splitComma(const std::string& s);
std::string                 toString(long n);

#endif
