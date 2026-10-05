#include "irc.hpp"
#include "Server.hpp"

static int ft_newport(const std::string& str)
{
    int port = 0;
    for (size_t i = 0; i < str.length(); i++)
    {
        if (!isdigit(static_cast<unsigned char>(str[i])) || port > 65535)
            return -1;
        port = port * 10 + (str[i] - '0');
    }
    if (port < 1 || port > 65535)
        return -1;
    return port;
}

int main(int argc, char **argv)
{
    int port;
    std::string password;

    if (argc != 3)
    {
        std::cerr << "Usage: ./ircserv <port> <password>\n";
        return 1;
    }
    if ((port = ft_newport(argv[1])) < 0)
    {
        std::cerr << "Invalid port\n";
        return 1;
    }
    password = argv[2];
    if (password.empty() || password.find_first_of(" \r\n") != std::string::npos)
    {
        std::cerr << "Invalid password\n";
        return 1;
    }
    try
    {
        Server serv(port, password);
        signal(SIGINT,  Server::handler);
        signal(SIGTERM, Server::handler);
        signal(SIGPIPE, SIG_IGN);
        if (!serv.init())
            return 1;
        if (!serv.run())
            return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
