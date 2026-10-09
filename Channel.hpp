#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include "irc.hpp"

class Channel
{
    private:
        std::string         name;
        std::string         topic;
        std::map<int, bool> members;    // fd -> opérateur ?
        std::set<int>       invited;
        bool                inviteOnly;         // +i
        bool                topicRestricted;    // +t
        std::string         key;                // +k (vide = pas de clé)
        size_t              limit;              // +l (0 = pas de limite)
    public:
        Channel();
        Channel(const std::string& name);
        Channel(const Channel& other);
        Channel& operator=(const Channel& other);
        ~Channel();

        const std::string&          getName() const;
        const std::string&          getTopic() const;
        void                        setTopic(const std::string& t);

        void                        addMember(int fd, bool op);
        void                        removeMember(int fd);
        bool                        isMember(int fd) const;
        bool                        isOperator(int fd) const;
        void                        setOperator(int fd, bool op);
        bool                        isEmpty() const;
        size_t                      size() const;
        const std::map<int, bool>&  getMembers() const;

        void    addInvite(int fd);
        void    removeInvite(int fd);
        bool    isInvited(int fd) const;

        bool                isInviteOnly() const;
        void                setInviteOnly(bool on);
        bool                isTopicRestricted() const;
        void                setTopicRestricted(bool on);
        const std::string&  getKey() const;
        void                setKey(const std::string& k);
        size_t              getLimit() const;
        void                setLimit(size_t l);
        std::string         modeString(bool showKey) const;
};

#endif
