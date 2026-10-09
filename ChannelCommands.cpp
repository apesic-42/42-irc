#include "Server.hpp"

static bool isChannelName(const std::string& name)
{
    return !name.empty() && (name[0] == '#' || name[0] == '&');
}

static bool validChannelName(const std::string& name)
{
    if (name.size() < 2 || name.size() > CHAN_LEN || !isChannelName(name))
        return false;
    return name.find_first_of(" ,:\a") == std::string::npos;
}

// JOIN #a,#b cle_a,cle_b   ou   JOIN 0 (quitter tous les canaux)
void Server::handleJoin(int fd, const std::vector<std::string>& params)
{
    if (params.empty() || params[0].empty())
    {
        reply(fd, "461", "JOIN :Not enough parameters");
        return ;
    }
    if (params[0] == "0")
    {
        std::vector<std::string> names;
        for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it)
        {
            if (it->second.isMember(fd))
                names.push_back(it->second.getName());
        }
        for (size_t k = 0; k < names.size(); k++)
            partChannel(fd, names[k], "Left all channels");
        return ;
    }
    std::vector<std::string> names = splitComma(params[0]);
    std::vector<std::string> keys;
    if (params.size() > 1)
        keys = splitComma(params[1]);
    for (size_t k = 0; k < names.size(); k++)
        joinChannel(fd, names[k], k < keys.size() ? keys[k] : "");
}

void Server::joinChannel(int fd, const std::string& name, const std::string& key)
{
    Client& c = clients[fd];

    if (!validChannelName(name))
    {
        reply(fd, "476", name + " :Bad Channel Mask");
        return ;
    }
    std::map<std::string, Channel>::iterator it = channels.find(ircLower(name));
    if (it == channels.end())
    {
        // Canal inexistant : on le crée, le premier arrivé est opérateur
        it = channels.insert(std::make_pair(ircLower(name), Channel(name))).first;
        it->second.addMember(fd, true);
    }
    else
    {
        Channel& ch = it->second;
        if (ch.isMember(fd))
            return ;
        if (ch.isInviteOnly() && !ch.isInvited(fd))
        {
            reply(fd, "473", ch.getName() + " :Cannot join channel (+i)");
            return ;
        }
        if (!ch.getKey().empty() && key != ch.getKey())
        {
            reply(fd, "475", ch.getName() + " :Cannot join channel (+k)");
            return ;
        }
        if (ch.getLimit() && ch.size() >= ch.getLimit())
        {
            reply(fd, "471", ch.getName() + " :Cannot join channel (+l)");
            return ;
        }
        ch.addMember(fd, false);
        ch.removeInvite(fd);
    }
    Channel& ch = it->second;
    broadcast(ch, ":" + c.prefix() + " JOIN :" + ch.getName() + "\r\n", -1);
    if (!ch.getTopic().empty())
        reply(fd, "332", ch.getName() + " :" + ch.getTopic());
    sendNames(fd, ch);
    sendTo(fd, ":" BOT_NICK "!bot@" SERVER_NAME " NOTICE " + c.getNick()
        + " :Welcome to " + ch.getName() + "! Type !help for bot commands.\r\n");
}

void Server::partChannel(int fd, const std::string& name, const std::string& reason)
{
    Channel* ch = findChannel(name);
    if (!ch)
    {
        reply(fd, "403", name + " :No such channel");
        return ;
    }
    if (!ch->isMember(fd))
    {
        reply(fd, "442", ch->getName() + " :You're not on that channel");
        return ;
    }
    std::string msg = ":" + clients[fd].prefix() + " PART " + ch->getName();
    if (!reason.empty())
        msg += " :" + reason;
    broadcast(*ch, msg + "\r\n", -1);
    ch->removeMember(fd);
    if (ch->isEmpty())
        channels.erase(ircLower(name));
}

void Server::handlePart(int fd, const std::vector<std::string>& params)
{
    if (params.empty() || params[0].empty())
    {
        reply(fd, "461", "PART :Not enough parameters");
        return ;
    }
    std::string reason = params.size() > 1 ? params[1] : "";
    std::vector<std::string> names = splitComma(params[0]);
    for (size_t k = 0; k < names.size(); k++)
        partChannel(fd, names[k], reason);
}

// 353 "= #canal :@op membre ..." découpé pour rester sous 512 octets, puis 366
void Server::sendNames(int fd, const Channel& ch)
{
    const std::map<int, bool>& members = ch.getMembers();
    std::string list;

    for (std::map<int, bool>::const_iterator it = members.begin(); it != members.end(); ++it)
    {
        std::string entry = (it->second ? "@" : "") + clients[it->first].getNick();
        if (!list.empty() && list.size() + entry.size() > 400)
        {
            reply(fd, "353", "= " + ch.getName() + " :" + list);
            list.clear();
        }
        if (!list.empty())
            list += " ";
        list += entry;
    }
    if (!list.empty())
        reply(fd, "353", "= " + ch.getName() + " :" + list);
    reply(fd, "366", ch.getName() + " :End of /NAMES list");
}

void Server::handleNames(int fd, const std::vector<std::string>& params)
{
    if (params.empty())
    {
        reply(fd, "366", "* :End of /NAMES list");
        return ;
    }
    std::vector<std::string> names = splitComma(params[0]);
    for (size_t k = 0; k < names.size(); k++)
    {
        Channel* ch = findChannel(names[k]);
        if (ch)
            sendNames(fd, *ch);
        else
            reply(fd, "366", names[k] + " :End of /NAMES list");
    }
}

void Server::handlePrivmsg(int fd, const std::vector<std::string>& params)
{
    sendMessage(fd, params, false);
}

void Server::handleNotice(int fd, const std::vector<std::string>& params)
{
    sendMessage(fd, params, true);
}

// PRIVMSG et NOTICE : même logique, mais NOTICE ne renvoie jamais d'erreur.
// Le texte est relayé tel quel : les requêtes CTCP/DCC (\x01...\x01) passent donc.
void Server::sendMessage(int fd, const std::vector<std::string>& params, bool notice)
{
    std::string cmd = notice ? "NOTICE" : "PRIVMSG";
    Client&     c = clients[fd];

    if (params.empty() || params[0].empty())
    {
        if (!notice)
            reply(fd, "411", ":No recipient given (" + cmd + ")");
        return ;
    }
    if (params.size() < 2 || params[1].empty())
    {
        if (!notice)
            reply(fd, "412", ":No text to send");
        return ;
    }
    const std::string& text = params[1];
    std::vector<std::string> targets = splitComma(params[0]);
    for (size_t k = 0; k < targets.size(); k++)
    {
        if (isChannelName(targets[k]))
        {
            Channel* ch = findChannel(targets[k]);
            if (!ch)
            {
                if (!notice)
                    reply(fd, "403", targets[k] + " :No such channel");
                continue ;
            }
            if (!ch->isMember(fd))
            {
                if (!notice)
                    reply(fd, "404", ch->getName() + " :Cannot send to channel");
                continue ;
            }
            broadcast(*ch, ":" + c.prefix() + " " + cmd + " " + ch->getName()
                + " :" + text + "\r\n", fd);
            if (!notice)
                botOnChannel(fd, *ch, text);
        }
        else if (ircLower(targets[k]) == ircLower(BOT_NICK))
        {
            if (!notice)
                botOnPrivate(fd, text);
        }
        else
        {
            int to = findFdByNick(targets[k]);
            if (to == -1)
            {
                if (!notice)
                    reply(fd, "401", targets[k] + " :No such nick/channel");
                continue ;
            }
            sendTo(to, ":" + c.prefix() + " " + cmd + " " + clients[to].getNick()
                + " :" + text + "\r\n");
        }
    }
}

// KICK #canal pseudo[,pseudo] [:raison]
void Server::handleKick(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (params.size() < 2)
    {
        reply(fd, "461", "KICK :Not enough parameters");
        return ;
    }
    Channel* ch = findChannel(params[0]);
    if (!ch)
    {
        reply(fd, "403", params[0] + " :No such channel");
        return ;
    }
    if (!ch->isMember(fd))
    {
        reply(fd, "442", ch->getName() + " :You're not on that channel");
        return ;
    }
    if (!ch->isOperator(fd))
    {
        reply(fd, "482", ch->getName() + " :You're not channel operator");
        return ;
    }
    std::string reason = (params.size() > 2 && !params[2].empty()) ? params[2] : c.getNick();
    std::vector<std::string> nicks = splitComma(params[1]);
    for (size_t k = 0; k < nicks.size(); k++)
    {
        int target = findFdByNick(nicks[k]);
        if (target == -1)
        {
            reply(fd, "401", nicks[k] + " :No such nick/channel");
            continue ;
        }
        if (!ch->isMember(target))
        {
            reply(fd, "441", clients[target].getNick() + " " + ch->getName()
                + " :They aren't on that channel");
            continue ;
        }
        broadcast(*ch, ":" + c.prefix() + " KICK " + ch->getName() + " "
            + clients[target].getNick() + " :" + reason + "\r\n", -1);
        ch->removeMember(target);
    }
    if (ch->isEmpty())
        channels.erase(ircLower(params[0]));
}

// INVITE pseudo #canal
void Server::handleInvite(int fd, const std::vector<std::string>& params)
{
    Client& c = clients[fd];

    if (params.size() < 2)
    {
        reply(fd, "461", "INVITE :Not enough parameters");
        return ;
    }
    int target = findFdByNick(params[0]);
    if (target == -1)
    {
        reply(fd, "401", params[0] + " :No such nick/channel");
        return ;
    }
    Channel* ch = findChannel(params[1]);
    if (!ch)
    {
        reply(fd, "403", params[1] + " :No such channel");
        return ;
    }
    if (!ch->isMember(fd))
    {
        reply(fd, "442", ch->getName() + " :You're not on that channel");
        return ;
    }
    if (ch->isInviteOnly() && !ch->isOperator(fd))
    {
        reply(fd, "482", ch->getName() + " :You're not channel operator");
        return ;
    }
    std::string targetNick = clients[target].getNick();
    if (ch->isMember(target))
    {
        reply(fd, "443", targetNick + " " + ch->getName() + " :is already on channel");
        return ;
    }
    ch->addInvite(target);
    reply(fd, "341", targetNick + " " + ch->getName());
    sendTo(target, ":" + c.prefix() + " INVITE " + targetNick + " :" + ch->getName() + "\r\n");
}

// TOPIC #canal          -> affiche le sujet
// TOPIC #canal :texte   -> le change (opérateur obligatoire si +t)
void Server::handleTopic(int fd, const std::vector<std::string>& params)
{
    if (params.empty())
    {
        reply(fd, "461", "TOPIC :Not enough parameters");
        return ;
    }
    Channel* ch = findChannel(params[0]);
    if (!ch)
    {
        reply(fd, "403", params[0] + " :No such channel");
        return ;
    }
    if (!ch->isMember(fd))
    {
        reply(fd, "442", ch->getName() + " :You're not on that channel");
        return ;
    }
    if (params.size() == 1)
    {
        if (ch->getTopic().empty())
            reply(fd, "331", ch->getName() + " :No topic is set");
        else
            reply(fd, "332", ch->getName() + " :" + ch->getTopic());
        return ;
    }
    if (ch->isTopicRestricted() && !ch->isOperator(fd))
    {
        reply(fd, "482", ch->getName() + " :You're not channel operator");
        return ;
    }
    std::string topic = params[1].substr(0, TOPIC_LEN);
    ch->setTopic(topic);
    broadcast(*ch, ":" + clients[fd].prefix() + " TOPIC " + ch->getName()
        + " :" + topic + "\r\n", -1);
}

void Server::handleMode(int fd, const std::vector<std::string>& params)
{
    if (params.empty())
    {
        reply(fd, "461", "MODE :Not enough parameters");
        return ;
    }
    if (!isChannelName(params[0]))
    {
        userMode(fd, params);
        return ;
    }
    Channel* ch = findChannel(params[0]);
    if (!ch)
    {
        reply(fd, "403", params[0] + " :No such channel");
        return ;
    }
    if (params.size() == 1)
    {
        reply(fd, "324", ch->getName() + " " + ch->modeString(ch->isMember(fd)));
        return ;
    }
    // Les clients demandent la liste des bannis en rejoignant : elle est toujours vide
    if (params[1] == "b" || params[1] == "+b")
    {
        reply(fd, "368", ch->getName() + " :End of channel ban list");
        return ;
    }
    if (!ch->isMember(fd))
    {
        reply(fd, "442", ch->getName() + " :You're not on that channel");
        return ;
    }
    if (!ch->isOperator(fd))
    {
        reply(fd, "482", ch->getName() + " :You're not channel operator");
        return ;
    }
    channelMode(fd, *ch, params);
}

// MODE #canal +itk-l cle : chaque lettre lit son paramètre dans l'ordre.
// Seuls les changements réels sont diffusés, en une seule ligne.
void Server::channelMode(int fd, Channel& ch, const std::vector<std::string>& params)
{
    const std::string&  modes = params[1];
    size_t              arg = 2;
    bool                adding = true;
    char                lastSign = 0;
    std::string         applied;
    std::string         appliedArgs;

    for (size_t k = 0; k < modes.size(); k++)
    {
        char m = modes[k];
        bool changed = false;
        std::string value;

        if (m == '+' || m == '-')
        {
            adding = (m == '+');
            continue ;
        }
        if (m == 'i')
        {
            changed = ch.isInviteOnly() != adding;
            ch.setInviteOnly(adding);
        }
        else if (m == 't')
        {
            changed = ch.isTopicRestricted() != adding;
            ch.setTopicRestricted(adding);
        }
        else if (m == 'k')
        {
            if (adding)
            {
                if (arg >= params.size() || params[arg].empty()
                    || params[arg].find_first_of(" ,") != std::string::npos)
                {
                    reply(fd, "461", "MODE :Not enough parameters");
                    arg++;
                    continue ;
                }
                value = params[arg++];
                changed = true;
                ch.setKey(value);
            }
            else
            {
                if (arg < params.size())
                    arg++;          // "-k cle" : la clé donnée est ignorée
                changed = !ch.getKey().empty();
                ch.setKey("");
            }
        }
        else if (m == 'l')
        {
            if (adding)
            {
                if (arg >= params.size())
                {
                    reply(fd, "461", "MODE :Not enough parameters");
                    continue ;
                }
                std::string s = params[arg++];
                if (s.empty() || s.size() > 6
                    || s.find_first_not_of("0123456789") != std::string::npos
                    || std::atol(s.c_str()) == 0)
                    continue ;
                size_t limit = std::atol(s.c_str());
                value = toString(limit);
                changed = true;
                ch.setLimit(limit);
            }
            else
            {
                changed = ch.getLimit() != 0;
                ch.setLimit(0);
            }
        }
        else if (m == 'o')
        {
            if (arg >= params.size())
            {
                reply(fd, "461", "MODE :Not enough parameters");
                continue ;
            }
            std::string nick = params[arg++];
            int target = findFdByNick(nick);
            if (target == -1)
            {
                reply(fd, "401", nick + " :No such nick/channel");
                continue ;
            }
            if (!ch.isMember(target))
            {
                reply(fd, "441", nick + " " + ch.getName() + " :They aren't on that channel");
                continue ;
            }
            value = clients[target].getNick();
            changed = ch.isOperator(target) != adding;
            ch.setOperator(target, adding);
        }
        else
        {
            reply(fd, "472", std::string(1, m) + " :is unknown mode char to me");
            continue ;
        }
        if (!changed)
            continue ;
        char sign = adding ? '+' : '-';
        if (sign != lastSign)
            applied += sign;
        lastSign = sign;
        applied += m;
        if (!value.empty())
            appliedArgs += " " + value;
    }
    if (!applied.empty())
        broadcast(ch, ":" + clients[fd].prefix() + " MODE " + ch.getName() + " "
            + applied + appliedArgs + "\r\n", -1);
}

// Pas de modes utilisateur : on accepte +i (envoyé par les clients) sans effet
void Server::userMode(int fd, const std::vector<std::string>& params)
{
    int target = findFdByNick(params[0]);
    if (target == -1)
    {
        reply(fd, "401", params[0] + " :No such nick/channel");
        return ;
    }
    if (target != fd)
    {
        reply(fd, "502", ":Can't change mode for other users");
        return ;
    }
    if (params.size() == 1)
    {
        reply(fd, "221", "+");
        return ;
    }
    if (params[1].find_first_not_of("+-i") != std::string::npos)
        reply(fd, "501", ":Unknown MODE flag");
}

// 352 : "<canal> <user> <host> <serveur> <nick> H[@] :0 <realname>"
void Server::handleWho(int fd, const std::vector<std::string>& params)
{
    std::string mask = params.empty() ? "*" : params[0];

    if (isChannelName(mask))
    {
        Channel* ch = findChannel(mask);
        if (ch)
        {
            const std::map<int, bool>& members = ch->getMembers();
            for (std::map<int, bool>::const_iterator it = members.begin(); it != members.end(); ++it)
            {
                Client& m = clients[it->first];
                reply(fd, "352", ch->getName() + " " + m.getUser() + " " + m.getHost()
                    + " " SERVER_NAME " " + m.getNick() + (it->second ? " H@" : " H")
                    + " :0 " + m.getRealname());
            }
        }
    }
    else
    {
        int target = findFdByNick(mask);
        if (target != -1)
        {
            Client& m = clients[target];
            reply(fd, "352", "* " + m.getUser() + " " + m.getHost() + " " SERVER_NAME " "
                + m.getNick() + " H :0 " + m.getRealname());
        }
    }
    reply(fd, "315", mask + " :End of WHO list");
}

void Server::handleList(int fd, const std::vector<std::string>& params)
{
    (void)params;
    reply(fd, "321", "Channel :Users  Name");
    for (std::map<std::string, Channel>::iterator it = channels.begin(); it != channels.end(); ++it)
        reply(fd, "322", it->second.getName() + " " + toString(it->second.size())
            + " :" + it->second.getTopic());
    reply(fd, "323", ":End of /LIST");
}

// Bonus : le bot répond dans le canal aux messages qui commencent par '!'
void Server::botOnChannel(int fd, const Channel& ch, const std::string& text)
{
    if (text.size() < 2 || text[0] != '!')
        return ;
    std::string answer = bot.answer(text.substr(1), clients[fd].getNick(),
                                    countUsers(), channels.size());
    if (answer.empty())
        return ;
    broadcast(ch, ":" BOT_NICK "!bot@" SERVER_NAME " PRIVMSG " + ch.getName()
        + " :" + answer + "\r\n", -1);
}

// Bonus : message privé au bot ("help" ou "!help")
void Server::botOnPrivate(int fd, const std::string& text)
{
    if (text[0] == '\x01')      // requête CTCP (VERSION...) : ignorée
        return ;
    std::string cmd = text[0] == '!' ? text.substr(1) : text;
    std::string answer = bot.answer(cmd, clients[fd].getNick(), countUsers(), channels.size());
    if (answer.empty())
        answer = "Unknown command. Try: help";
    sendTo(fd, ":" BOT_NICK "!bot@" SERVER_NAME " PRIVMSG " + clients[fd].getNick()
        + " :" + answer + "\r\n");
}
