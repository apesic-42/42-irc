#include "Bot.hpp"

Bot::Bot()
{
    std::srand(static_cast<unsigned int>(std::time(NULL)));
}
Bot::Bot(const Bot& other)
{
    (void)other;
}
Bot& Bot::operator=(const Bot& other)
{
    (void)other;
    return *this;
}
Bot::~Bot() {}

// text : la commande sans le '!' (ex : "roll 20"). Chaîne vide = pas de réponse.
std::string Bot::answer(const std::string& text, const std::string& asker,
                        size_t nbUsers, size_t nbChannels) const
{
    std::istringstream iss(text);
    std::string cmd;
    iss >> cmd;
    cmd = ircLower(cmd);

    if (cmd == "help")
        return "Commands: !help, !time, !roll [max], !users, !ping";
    if (cmd == "ping")
        return "pong!";
    if (cmd == "time")
    {
        char buf[64];
        time_t now = std::time(NULL);
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string("Server time: ") + buf;
    }
    if (cmd == "users")
        return toString(nbUsers) + " user(s) connected, "
            + toString(nbChannels) + " channel(s) open";
    if (cmd == "roll")
    {
        long max = 6;
        if (!(iss >> max) || max < 2 || max > 1000000)
            max = 6;
        return asker + " rolled " + toString(std::rand() % max + 1)
            + " (1-" + toString(max) + ")";
    }
    return "";
}
