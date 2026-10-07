#include "irc.hpp"

// Casemapping rfc1459 : en IRC, []\~ sont les majuscules de {}|^
std::string ircLower(const std::string& s)
{
    std::string res = s;
    for (size_t i = 0; i < res.size(); i++)
    {
        if (res[i] == '[')
            res[i] = '{';
        else if (res[i] == ']')
            res[i] = '}';
        else if (res[i] == '\\')
            res[i] = '|';
        else if (res[i] == '~')
            res[i] = '^';
        else
            res[i] = std::tolower(static_cast<unsigned char>(res[i]));
    }
    return res;
}

std::string toUpper(const std::string& s)
{
    std::string res = s;
    for (size_t i = 0; i < res.size(); i++)
        res[i] = std::toupper(static_cast<unsigned char>(res[i]));
    return res;
}

// "#a,#b,,#c" -> ["#a", "#b", "#c"]
std::vector<std::string> splitComma(const std::string& s)
{
    std::vector<std::string> res;
    std::string item;
    std::istringstream iss(s);
    while (std::getline(iss, item, ','))
    {
        if (!item.empty())
            res.push_back(item);
    }
    return res;
}

std::string toString(long n)
{
    std::ostringstream oss;
    oss << n;
    return oss.str();
}
