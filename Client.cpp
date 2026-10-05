#include "Client.hpp"

Client::Client()
    : fd(-1), passOk(false), capNegotiating(false), registered(false),
      toRemove(false), quitDone(false), removeTime(0) {}

Client::Client(int fd, const std::string& host)
    : fd(fd), host(host), passOk(false), capNegotiating(false), registered(false),
      toRemove(false), quitDone(false), removeTime(0) {}

Client::Client(const Client& other)
{
    *this = other;
}

// Ne ferme jamais le fd : c'est le Server qui en est propriétaire
Client& Client::operator=(const Client& other)
{
    if (this != &other)
    {
        fd = other.fd;
        host = other.host;
        nick = other.nick;
        user = other.user;
        realname = other.realname;
        inbuf = other.inbuf;
        outbuf = other.outbuf;
        passOk = other.passOk;
        capNegotiating = other.capNegotiating;
        registered = other.registered;
        toRemove = other.toRemove;
        quitDone = other.quitDone;
        quitReason = other.quitReason;
        removeTime = other.removeTime;
    }
    return *this;
}

Client::~Client() {}

int Client::getFd() const
{
    return fd;
}
const std::string& Client::getHost() const
{
    return host;
}
const std::string& Client::getNick() const
{
    return nick;
}
const std::string& Client::getUser() const
{
    return user;
}
const std::string& Client::getRealname() const
{
    return realname;
}
// Identité de l'émetteur dans les messages relayés : nick!user@host
std::string Client::prefix() const
{
    return nick + "!" + user + "@" + host;
}
void Client::setNick(const std::string& n)
{
    nick = n;
}
void Client::setUser(const std::string& u)
{
    user = u;
}
void Client::setRealname(const std::string& r)
{
    realname = r;
}

bool Client::isPassOk() const
{
    return passOk;
}
void Client::setPassOk(bool ok)
{
    passOk = ok;
}
bool Client::isCapNegotiating() const
{
    return capNegotiating;
}
void Client::setCapNegotiating(bool on)
{
    capNegotiating = on;
}
bool Client::isRegistered() const
{
    return registered;
}
void Client::setRegistered(bool reg)
{
    registered = reg;
}

void Client::appendData(const char* data, size_t n)
{
    inbuf.append(data, n);
}

bool Client::extractLine(std::string& line)
{
    size_t pos = inbuf.find('\n');
    if (pos == std::string::npos)
        return false;
    line = inbuf.substr(0, pos);
    inbuf.erase(0, pos + 1);
    if (!line.empty() && line[line.size() - 1] == '\r')
        line.erase(line.size() - 1);
    return true;
}

size_t Client::inSize() const
{
    return inbuf.size();
}

void Client::queueReply(const std::string& msg)
{
    outbuf.append(msg);
}
void Client::consumeOut(size_t n)
{
    outbuf.erase(0, n);
}
void Client::clearOut()
{
    outbuf.clear();
}
bool Client::hasPending() const
{
    return !outbuf.empty();
}
size_t Client::outSize() const
{
    return outbuf.size();
}
const std::string& Client::getOutbuf() const
{
    return outbuf;
}

// Seule la première raison compte : un client ne part qu'une fois
void Client::markForRemoval(const std::string& reason)
{
    if (toRemove)
        return ;
    toRemove = true;
    quitReason = reason;
    removeTime = std::time(NULL);
}
bool Client::isToRemove() const
{
    return toRemove;
}
bool Client::isQuitDone() const
{
    return quitDone;
}
void Client::setQuitDone()
{
    quitDone = true;
}
const std::string& Client::getQuitReason() const
{
    return quitReason;
}
time_t Client::getRemoveTime() const
{
    return removeTime;
}
