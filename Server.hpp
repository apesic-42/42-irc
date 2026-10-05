#ifndef SERVER_HPP
#define SERVER_HPP

#include "irc.hpp"
#include "Client.hpp"
#include "Channel.hpp"
#include "Bot.hpp"

class Server;
typedef void (Server::*Handler)(int fd, const std::vector<std::string>& params);

class Server
{
    private :
        int                             server_fd;
        int                             port;
        std::string                     pass;
        std::string                     created;
        std::vector<struct pollfd>      ports;
        std::map<int, Client>           clients;
        std::map<std::string, Channel>  channels;   // clé : nom en minuscules
        std::map<std::string, Handler>  commands;
        Bot                             bot;

        // Non copiable : un seul objet possède les fds
        Server(const Server& serv);
        Server& operator=(const Server& serv);

        // Réseau (Server.cpp)
        int     acceptClient();
        void    handleEvents(size_t i);
        void    readFromClient(size_t i);
        void    flushClient(size_t i);
        void    processLine(int fd, const std::string& line);
        void    cleanupQuits();
        void    removeClients();
        void    removeClient(size_t i);
        int     pollTimeout() const;
        int     findPort(int fd) const;

        // Outils d'envoi et de recherche (Commands.cpp)
        void        initCommands();
        bool        parseLine(const std::string& line, std::string& cmd,
                              std::vector<std::string>& params) const;
        void        executeCommand(int fd, const std::string& cmd,
                                   const std::vector<std::string>& params);
        void        sendTo(int fd, const std::string& msg);
        void        reply(int fd, const std::string& code, const std::string& args);
        void        broadcast(const Channel& ch, const std::string& msg, int except);
        void        sendToPeers(int fd, const std::string& msg);
        void        disconnect(int fd, const std::string& reason);
        void        quitChannels(int fd, const std::string& reason);
        int         findFdByNick(const std::string& nick) const;
        Channel*    findChannel(const std::string& name);
        void        tryRegister(int fd);
        size_t      countUsers() const;

        // Enregistrement (Commands.cpp)
        void    handleCap(int fd, const std::vector<std::string>& params);
        void    handlePass(int fd, const std::vector<std::string>& params);
        void    handleNick(int fd, const std::vector<std::string>& params);
        void    handleUser(int fd, const std::vector<std::string>& params);
        void    handlePing(int fd, const std::vector<std::string>& params);
        void    handlePong(int fd, const std::vector<std::string>& params);
        void    handleQuit(int fd, const std::vector<std::string>& params);

        // Canaux (ChannelCommands.cpp)
        void    joinChannel(int fd, const std::string& name, const std::string& key);
        void    partChannel(int fd, const std::string& name, const std::string& reason);
        void    sendNames(int fd, const Channel& ch);
        void    sendMessage(int fd, const std::vector<std::string>& params, bool notice);
        void    channelMode(int fd, Channel& ch, const std::vector<std::string>& params);
        void    userMode(int fd, const std::vector<std::string>& params);
        void    handleJoin(int fd, const std::vector<std::string>& params);
        void    handlePart(int fd, const std::vector<std::string>& params);
        void    handlePrivmsg(int fd, const std::vector<std::string>& params);
        void    handleNotice(int fd, const std::vector<std::string>& params);
        void    handleKick(int fd, const std::vector<std::string>& params);
        void    handleInvite(int fd, const std::vector<std::string>& params);
        void    handleTopic(int fd, const std::vector<std::string>& params);
        void    handleMode(int fd, const std::vector<std::string>& params);
        void    handleNames(int fd, const std::vector<std::string>& params);
        void    handleWho(int fd, const std::vector<std::string>& params);
        void    handleList(int fd, const std::vector<std::string>& params);

        // Bonus : bot (ChannelCommands.cpp)
        void    botOnChannel(int fd, const Channel& ch, const std::string& text);
        void    botOnPrivate(int fd, const std::string& text);

    public :
        static volatile sig_atomic_t flag_stop;
        static void handler(int sig);

        Server(int port, const std::string& pass);
        ~Server();
        int init();
        int run();
};

#endif
