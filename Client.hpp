#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "irc.hpp"

class Client
{
    private:
        int         fd;
        std::string host;
        std::string nick;
        std::string user;
        std::string realname;
        std::string inbuf;
        std::string outbuf;
        bool        passOk;
        bool        capNegotiating;
        bool        registered;
        bool        toRemove;
        bool        quitDone;
        std::string quitReason;
        time_t      removeTime;
    public:
        Client();
        Client(int fd, const std::string& host);
        Client(const Client& other);
        Client& operator=(const Client& other);
        ~Client();

        int                 getFd() const;
        const std::string&  getHost() const;
        const std::string&  getNick() const;
        const std::string&  getUser() const;
        const std::string&  getRealname() const;
        std::string         prefix() const;
        void                setNick(const std::string& n);
        void                setUser(const std::string& u);
        void                setRealname(const std::string& r);

        bool    isPassOk() const;
        void    setPassOk(bool ok);
        bool    isCapNegotiating() const;
        void    setCapNegotiating(bool on);
        bool    isRegistered() const;
        void    setRegistered(bool reg);

        // tampon d'entrée
        void    appendData(const char* data, size_t n);
        bool    extractLine(std::string& line);
        size_t  inSize() const;

        // tampon de sortie
        void                queueReply(const std::string& msg);
        void                consumeOut(size_t n);
        void                clearOut();
        bool                hasPending() const;
        size_t              outSize() const;
        const std::string&  getOutbuf() const;

        // déconnexion différée
        void                markForRemoval(const std::string& reason);
        bool                isToRemove() const;
        bool                isQuitDone() const;
        void                setQuitDone();
        const std::string&  getQuitReason() const;
        time_t              getRemoveTime() const;
};

#endif
