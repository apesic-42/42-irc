#ifndef BOT_HPP
#define BOT_HPP

#include "irc.hpp"

// Bonus : utilisateur virtuel du serveur (pas de socket, pas de fd).
// Il reçoit le texte d'une commande et renvoie la réponse à afficher.
class Bot
{
    public:
        Bot();
        Bot(const Bot& other);
        Bot& operator=(const Bot& other);
        ~Bot();

        std::string answer(const std::string& text, const std::string& asker,
                           size_t nbUsers, size_t nbChannels) const;
};

#endif
