#include "Server.hpp"

volatile sig_atomic_t Server::flag_stop = 0;

Server::Server(int port, const std::string& pass) : server_fd(-1), port(port), pass(pass)
{
    char buf[64];
    time_t now = std::time(NULL);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    created = buf;
    initCommands();
}

Server::~Server()
{
    size_t i = 0;
    while (i < ports.size())
    {
        close(ports[i].fd);
        i++;
    }
}

void Server::handler(int sig)
{
    (void)sig;
    flag_stop = 1;
}

int Server::init()
{
    struct sockaddr_in addr = {};

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        std::cerr << "Socket error\n";
        return 0;
    }
    int state = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &state, sizeof(int)))
    {
        std::cerr << "setsockopt error\n";
        close(server_fd);
        return 0;
    }
    if (fcntl(server_fd, F_SETFL, O_NONBLOCK))
    {
        std::cerr << "Server fcntl error.\n";
        close(server_fd);
        return 0;
    }
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        std::cerr << "Bind failed\n";
        close(server_fd);
        return 0;
    }
    if (listen(server_fd, SOMAXCONN) == -1)
    {
        std::cerr << "Listen failed\n";
        close(server_fd);
        return 0;
    }
    struct pollfd pfd = {server_fd, POLLIN, 0};
    ports.push_back(pfd);
    std::cout << "Listening on port " << port << std::endl;
    return 1;
}

int Server::run()
{
    while (!flag_stop)
    {
        int ret = poll(&ports[0], ports.size(), pollTimeout());
        if (ret == -1)
        {
            if (flag_stop)
                break ;
            std::cerr << "Poll error.\n";
            return 0;
        }
        // Les clients acceptés pendant ce tour seront traités au suivant
        size_t count = ports.size();
        for (size_t i = 0; i < count; i++)
        {
            if (ports[i].revents == 0)
                continue ;
            if (ports[i].fd == server_fd)
            {
                if (ports[i].revents & POLLIN)
                    acceptClient();
                continue ;
            }
            try
            {
                handleEvents(i);
            }
            catch (const std::exception& e)
            {
                // Plus de mémoire pour ce client : on le sacrifie, pas le serveur
                Client& c = clients[ports[i].fd];
                c.clearOut();
                c.markForRemoval("Server error");
            }
        }
        cleanupQuits();
        removeClients();
    }
    std::cout << "\nServer stopped." << std::endl;
    return 1;
}

int Server::acceptClient()
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    int client_fd = accept(server_fd, (struct sockaddr *)&addr, &len);
    if (client_fd == -1)
        return 0;
    if (fcntl(client_fd, F_SETFL, O_NONBLOCK))
    {
        std::cerr << "Client fcntl error.\n";
        close(client_fd);
        return 0;
    }
    try
    {
        struct pollfd client_pfd = {client_fd, POLLIN, 0};
        clients[client_fd] = Client(client_fd, inet_ntoa(addr.sin_addr));
        ports.push_back(client_pfd);
    }
    catch (const std::exception& e)
    {
        clients.erase(client_fd);
        close(client_fd);
        return 0;
    }
    std::cout << "New client (fd " << client_fd << ") from "
              << clients[client_fd].getHost() << std::endl;
    return 1;
}

// Un seul point d'entrée par fd et par tour de poll
void Server::handleEvents(size_t i)
{
    short   rev = ports[i].revents;
    Client& c = clients[ports[i].fd];

    if (rev & (POLLERR | POLLNVAL))
    {
        c.clearOut();
        c.markForRemoval("Connection error");
        return ;
    }
    if (rev & POLLIN)
        readFromClient(i);
    else if (rev & POLLHUP)
    {
        c.clearOut();
        c.markForRemoval("Connection closed");
        return ;
    }
    if ((rev & POLLOUT) && c.hasPending())
        flushClient(i);
}

void Server::readFromClient(size_t i)
{
    int     fd = ports[i].fd;
    Client& c = clients[fd];
    char    buffer[1024];

    if (c.isToRemove())
        return ;
    ssize_t bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0)
    {
        // 0 : le client a fermé ; -1 : erreur. Dans les deux cas il est perdu.
        c.clearOut();
        c.markForRemoval(bytes == 0 ? "Connection closed" : "Read error");
        return ;
    }
    c.appendData(buffer, bytes);
    std::string line;
    while (!c.isToRemove() && c.extractLine(line))
        processLine(fd, line);
    // Ce qui reste est une ligne sans \n : trop longue = client abusif
    if (!c.isToRemove() && c.inSize() > MAX_LINE)
        disconnect(fd, "Input line too long");
}

void Server::flushClient(size_t i)
{
    int     fd = ports[i].fd;
    Client& c = clients[fd];

    ssize_t sent = send(fd, c.getOutbuf().data(), c.getOutbuf().size(), 0);
    if (sent <= 0)
    {
        c.clearOut();
        c.markForRemoval("Write error");
        return ;
    }
    c.consumeOut(sent);
    if (!c.hasPending())
        ports[i].events &= ~POLLOUT;     // sinon poll répond "prêt" en boucle
}

void Server::processLine(int fd, const std::string& line)
{
    std::string                 cmd;
    std::vector<std::string>    params;

    std::cout << "<< " << fd << ": " << line << std::endl;
    // 512 octets max, \r\n compris : on tronque le surplus
    if (!parseLine(line.substr(0, MAX_LINE - 2), cmd, params))
        return ;
    executeCommand(fd, cmd, params);
}

// Les clients marqués (QUIT, erreur, débordement...) quittent leurs canaux
// ici, hors de tout parcours de canal. Un QUIT diffusé peut à son tour
// faire déborder un autre client : on boucle jusqu'à stabilité.
void Server::cleanupQuits()
{
    bool again = true;
    while (again)
    {
        again = false;
        for (std::map<int, Client>::iterator it = clients.begin(); it != clients.end(); ++it)
        {
            if (it->second.isToRemove() && !it->second.isQuitDone())
            {
                it->second.setQuitDone();
                quitChannels(it->first, it->second.getQuitReason());
                again = true;
            }
        }
    }
    // Un client sortant n'est plus lu, seulement vidé
    for (size_t i = 1; i < ports.size(); i++)
    {
        Client& c = clients[ports[i].fd];
        if (c.isToRemove())
            ports[i].events = c.hasPending() ? POLLOUT : 0;
    }
}

void Server::removeClients()
{
    time_t now = std::time(NULL);

    for (size_t i = ports.size(); i-- > 1; )
    {
        Client& c = clients[ports[i].fd];
        if (c.isToRemove() && (!c.hasPending() || now - c.getRemoveTime() >= QUIT_DELAY))
            removeClient(i);
    }
}

void Server::removeClient(size_t i)
{
    int fd = ports[i].fd;
    std::cout << "Client fd " << fd << " disconnected ("
              << clients[fd].getQuitReason() << ")" << std::endl;
    close(fd);
    clients.erase(fd);
    ports.erase(ports.begin() + i);
}

// Un client sortant a encore des messages : on se réveille pour respecter QUIT_DELAY
int Server::pollTimeout() const
{
    for (std::map<int, Client>::const_iterator it = clients.begin(); it != clients.end(); ++it)
    {
        if (it->second.isToRemove())
            return 1000;
    }
    return -1;
}

int Server::findPort(int fd) const
{
    for (size_t i = 0; i < ports.size(); i++)
    {
        if (ports[i].fd == fd)
            return i;
    }
    return -1;
}
