#include "Server.hpp"

void Server::initCommands()
{
    commands["CAP"] = &Server::handleCap;
    commands["PASS"] = &Server::handlePass;
    commands["NICK"] = &Server::handleNick;
    commands["USER"] = &Server::handleUser;
    commands["PING"] = &Server::handlePing;
    commands["PONG"] = &Server::handlePong;
    commands["QUIT"] = &Server::handleQuit;
    commands["JOIN"] = &Server::handleJoin;
    commands["PART"] = &Server::handlePart;
    commands["PRIVMSG"] = &Server::handlePrivmsg;
    commands["NOTICE"] = &Server::handleNotice;
    commands["KICK"] = &Server::handleKick;
    commands["INVITE"] = &Server::handleInvite;
    commands["TOPIC"] = &Server::handleTopic;
    commands["MODE"] = &Server::handleMode;
    commands["NAMES"] = &Server::handleNames;
    commands["WHO"] = &Server::handleWho;
    commands["LIST"] = &Server::handleList;
}

// "[:prefixe] COMMANDE p1 p2 :dernier param avec espaces"
bool Server::parseLine(const std::string& line, std::string& cmd,
                       std::vector<std::string>& params) const
{
    size_t i = 0;
    size_t n = line.size();

    while (i < n && line[i] == ' ')
        i++;
    if (i < n && line[i] == ':')
    {
        while (i < n && line[i] != ' ')
            i++;
        while (i < n && line[i] == ' ')
            i++;
    }
    size_t start = i;
    while (i < n && line[i] != ' ')
        i++;
    cmd = toUpper(line.substr(start, i - start));
    if (cmd.empty())
        return false;
    while (i < n)
    {
        while (i < n && line[i] == ' ')
            i++;
        if (i >= n)
            break ;
        if (line[i] == ':')
        {
            params.push_back(line.substr(i + 1));
            break ;
        }
        start = i;
        while (i < n && line[i] != ' ')
            i++;
        params.push_back(line.substr(start, i - start));
    }
    return true;
}

void Server::executeCommand(int fd, const std::string& cmd,
                            const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (!c.isRegistered() && cmd != "CAP" && cmd != "PASS" && cmd != "NICK"
        && cmd != "USER" && cmd != "PING" && cmd != "PONG" && cmd != "QUIT")
    {
        reply(fd, "451", ":You have not registered");
        return ;
    }
    std::map<std::string, Handler>::iterator it = commands.find(cmd);
    if (it == commands.end())
    {
        reply(fd, "421", cmd + " :Unknown command");
        return ;
    }
    (this->*(it->second))(fd, params);
}

// Tout envoi passe par ici : on met en file, poll() dira quand envoyer
void Server::sendTo(int fd, const std::string& msg)
{
    std::map<int, Client>::iterator it = clients.find(fd);
    if (it == clients.end())
        return ;
    Client& c = it->second;
    if (c.outSize() + msg.size() > MAX_OUTBUF)
    {
        // Le client ne lit plus : on ne laisse pas sa file grossir sans fin
        if (!c.isToRemove())
        {
            c.clearOut();
            c.markForRemoval("SendQ exceeded");
        }
        return ;
    }
    c.queueReply(msg);
    int i = findPort(fd);
    if (i != -1)
        ports[i].events |= POLLOUT;
}

// :ircserv CODE cible arguments\r\n  (cible = pseudo, ou * avant NICK)
void Server::reply(int fd, const std::string& code, const std::string& args)
{
    std::string nick = clients[fd].getNick();
    if (nick.empty())
        nick = "*";
    sendTo(fd, ":" SERVER_NAME " " + code + " " + nick + " " + args + "\r\n");
}

void Server::broadcast(const Channel& ch, const std::string& msg, int except)
{
    const std::map<int, bool>& members = ch.getMembers();
    for (std::map<int, bool>::const_iterator it = members.begin(); it != members.end(); ++it)
    {
        if (it->first != except)
            sendTo(it->first, msg);
    }
}

// Envoie une seule fois à chaque client qui partage au moins un canal avec fd
void Server::sendToPeers(int fd, const std::string& msg)
{
    std::set<int> peers;
    for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it)
    {
        if (!it->second.isMember(fd))
            continue ;
        const std::map<int, bool>& members = it->second.getMembers();
        for (std::map<int, bool>::const_iterator m = members.begin(); m != members.end(); ++m)
            peers.insert(m->first);
    }
    peers.erase(fd);
    for (std::set<int>::iterator it = peers.begin(); it != peers.end(); ++it)
        sendTo(*it, msg);
}

// Fermeture demandée par le serveur ou par QUIT : ERROR puis départ différé
void Server::disconnect(int fd, const std::string& reason)
{
    Client& c = clients[fd];
    sendTo(fd, "ERROR :Closing link: (" + c.getUser() + "@" + c.getHost()
        + ") [" + reason + "]\r\n");
    c.markForRemoval(reason);
}

// Appelé une seule fois par client sortant (voir cleanupQuits)
void Server::quitChannels(int fd, const std::string& reason)
{
    Client& c = clients[fd];
    if (c.isRegistered())
        sendToPeers(fd, ":" + c.prefix() + " QUIT :" + reason + "\r\n");
    std::map<std::string, Channel>::iterator it = channels.begin();
    while (it != channels.end())
    {
        it->second.removeMember(fd);
        it->second.removeInvite(fd);   // le fd pourra être réattribué à un autre client
        if (it->second.isEmpty())
            channels.erase(it++);
        else
            ++it;
    }
}

int Server::findFdByNick(const std::string& nick) const
{
    std::string low = ircLower(nick);
    for (std::map<int, Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (!it->second.isToRemove() && !it->second.getNick().empty()
            && ircLower(it->second.getNick()) == low)
            return it->first;
    }
    return -1;
}

Channel* Server::findChannel(const std::string& name)
{
    std::map<std::string, Channel>::iterator it = channels.find(ircLower(name));
    if (it == channels.end())
        return NULL;
    return &it->second;
}

size_t Server::countUsers() const
{
    size_t n = 0;
    for (std::map<int, Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->second.isRegistered() && !it->second.isToRemove())
            n++;
    }
    return n;
}

void Server::tryRegister(int fd)
{
    Client& c = clients[fd];

    if (c.isRegistered() || c.getNick().empty() || c.getUser().empty()
        || c.isCapNegotiating())
        return ;
    if (!c.isPassOk())
    {
        reply(fd, "464", ":Password incorrect");
        disconnect(fd, "Password required");
        return ;
    }
    c.setRegistered(true);
    reply(fd, "001", ":Welcome to the ft_irc Network, " + c.prefix());
    reply(fd, "002", ":Your host is " SERVER_NAME ", running version " VERSION);
    reply(fd, "003", ":This server was created " + created);
    reply(fd, "004", SERVER_NAME " " VERSION " i iklot");
    reply(fd, "005", "CHANTYPES=# PREFIX=(o)@ CHANMODES=,k,l,it NICKLEN="
        + toString(NICK_LEN) + " CHANNELLEN=" + toString(CHAN_LEN) + " TOPICLEN="
        + toString(TOPIC_LEN) + " CASEMAPPING=rfc1459 :are supported by this server");
    reply(fd, "422", ":MOTD File is missing");
}

// Les vrais clients ouvrent par CAP LS : on n'offre aucune capacité,
// et on attend CAP END pour terminer l'enregistrement.
void Server::handleCap(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];
    std::string nick = c.getNick().empty() ? "*" : c.getNick();

    if (params.empty())
    {
        reply(fd, "461", "CAP :Not enough parameters");
        return ;
    }
    std::string sub = toUpper(params[0]);
    if (sub == "LS" || sub == "LIST")
    {
        if (sub == "LS" && !c.isRegistered())
            c.setCapNegotiating(true);
        sendTo(fd, ":" SERVER_NAME " CAP " + nick + " " + sub + " :\r\n");
    }
    else if (sub == "REQ")
        sendTo(fd, ":" SERVER_NAME " CAP " + nick + " NAK :"
            + (params.size() > 1 ? params[1] : "") + "\r\n");
    else if (sub == "END")
    {
        c.setCapNegotiating(false);
        tryRegister(fd);
    }
    else
        reply(fd, "410", sub + " :Invalid CAP command");
}

void Server::handlePass(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (c.isRegistered())
    {
        reply(fd, "462", ":You may not reregister");
        return ;
    }
    if (params.empty())
    {
        reply(fd, "461", "PASS :Not enough parameters");
        return ;
    }
    if (params[0] != pass)
    {
        reply(fd, "464", ":Password incorrect");
        disconnect(fd, "Bad password");
        return ;
    }
    c.setPassOk(true);
}

// 1er caractère : lettre ou spécial ; ensuite lettres, chiffres, spéciaux ou '-'
static bool validNick(const std::string& nick)
{
    const std::string special = "[]\\`_^{|}";

    if (nick.empty() || nick.size() > NICK_LEN)
        return false;
    if (!std::isalpha(static_cast<unsigned char>(nick[0]))
        && special.find(nick[0]) == std::string::npos)
        return false;
    for (size_t i = 1; i < nick.size(); i++)
    {
        if (!std::isalnum(static_cast<unsigned char>(nick[i]))
            && special.find(nick[i]) == std::string::npos && nick[i] != '-')
            return false;
    }
    return true;
}

void Server::handleNick(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (params.empty() || params[0].empty())
    {
        reply(fd, "431", ":No nickname given");
        return ;
    }
    std::string nick = params[0];
    if (!validNick(nick))
    {
        reply(fd, "432", nick + " :Erroneous nickname");
        return ;
    }
    int other = findFdByNick(nick);
    if ((other != -1 && other != fd) || ircLower(nick) == ircLower(BOT_NICK))
    {
        reply(fd, "433", nick + " :Nickname is already in use");
        return ;
    }
    if (nick == c.getNick())
        return ;
    if (c.isRegistered())
    {
        std::string msg = ":" + c.prefix() + " NICK :" + nick + "\r\n";
        sendTo(fd, msg);
        sendToPeers(fd, msg);
    }
    c.setNick(nick);
    tryRegister(fd);
}

// USER <username> <mode> <unused> :<realname>
void Server::handleUser(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (c.isRegistered())
    {
        reply(fd, "462", ":You may not reregister");
        return ;
    }
    if (params.size() < 4 || params[0].empty())
    {
        reply(fd, "461", "USER :Not enough parameters");
        return ;
    }
    c.setUser(params[0].substr(0, 10));
    c.setRealname(params[3]);
    tryRegister(fd);
}

void Server::handlePing(int fd, const std::vector<std::string>& params)
{
    if (params.empty())
    {
        reply(fd, "409", ":No origin specified");
        return ;
    }
    sendTo(fd, ":" SERVER_NAME " PONG " SERVER_NAME " :" + params[0] + "\r\n");
}

void Server::handlePong(int fd, const std::vector<std::string>& params)
{
    (void)fd;
    (void)params;
}

void Server::handleQuit(int fd, const std::vector<std::string>& params)
{
    std::string reason = "Client Quit";
    if (!params.empty() && !params[0].empty())
        reason = "Quit: " + params[0];
    disconnect(fd, reason);
}
