#include "Channel.hpp"

Channel::Channel() : inviteOnly(false), topicRestricted(false), limit(0) {}

Channel::Channel(const std::string& name)
    : name(name), inviteOnly(false), topicRestricted(false), limit(0) {}

Channel::Channel(const Channel& other)
{
    *this = other;
}

Channel& Channel::operator=(const Channel& other)
{
    if (this != &other)
    {
        name = other.name;
        topic = other.topic;
        members = other.members;
        invited = other.invited;
        inviteOnly = other.inviteOnly;
        topicRestricted = other.topicRestricted;
        key = other.key;
        limit = other.limit;
    }
    return *this;
}

Channel::~Channel() {}

const std::string& Channel::getName() const
{
    return name;
}
const std::string& Channel::getTopic() const
{
    return topic;
}
void Channel::setTopic(const std::string& t)
{
    topic = t;
}

void Channel::addMember(int fd, bool op)
{
    members[fd] = op;
}
void Channel::removeMember(int fd)
{
    members.erase(fd);
}
bool Channel::isMember(int fd) const
{
    return members.find(fd) != members.end();
}
bool Channel::isOperator(int fd) const
{
    std::map<int, bool>::const_iterator it = members.find(fd);
    return it != members.end() && it->second;
}
void Channel::setOperator(int fd, bool op)
{
    std::map<int, bool>::iterator it = members.find(fd);
    if (it != members.end())
        it->second = op;
}
bool Channel::isEmpty() const
{
    return members.empty();
}
size_t Channel::size() const
{
    return members.size();
}
const std::map<int, bool>& Channel::getMembers() const
{
    return members;
}

void Channel::addInvite(int fd)
{
    invited.insert(fd);
}
void Channel::removeInvite(int fd)
{
    invited.erase(fd);
}
bool Channel::isInvited(int fd) const
{
    return invited.find(fd) != invited.end();
}

bool Channel::isInviteOnly() const
{
    return inviteOnly;
}
void Channel::setInviteOnly(bool on)
{
    inviteOnly = on;
}
bool Channel::isTopicRestricted() const
{
    return topicRestricted;
}
void Channel::setTopicRestricted(bool on)
{
    topicRestricted = on;
}
const std::string& Channel::getKey() const
{
    return key;
}
void Channel::setKey(const std::string& k)
{
    key = k;
}
size_t Channel::getLimit() const
{
    return limit;
}
void Channel::setLimit(size_t l)
{
    limit = l;
}

// Ex : "+itkl secret 10". La clé n'est montrée qu'aux membres.
std::string Channel::modeString(bool showKey) const
{
    std::string modes = "+";
    std::string args;

    if (inviteOnly)
        modes += "i";
    if (topicRestricted)
        modes += "t";
    if (!key.empty())
    {
        modes += "k";
        args += " " + (showKey ? key : std::string("*"));
    }
    if (limit)
    {
        modes += "l";
        args += " " + toString(limit);
    }
    return modes + args;
}
