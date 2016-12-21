# A finir
- Corriger le fait que l'identifiant du premier transfert soit toujours 59
- Vérifier que les pseudo ne contiennent pas d'espace
- Toujours ajouter `'\0'` aux requêtes lues par le serveur

# Base
- [x] Démarrage / fin propre du serveur
    - [x] Lance l'exécutable
    - [x] Affiche les informations relatives au serveur
    - On peut taper les commandes :
        - [x] stop : arrête le serveur
- [x] Connexion au serveur / déconnexion propre du serveur
- [x] Demande de connexion / déconnexion depuis le client
- [x] Réception et redistribution des messages depuis le serveur
- [x] Réception de messages de la part de clients
- [x] Envoi de messages de la part de clients
- [x] Affichage sous forme de tableau des messages entrant/sortant

# Fonctionnalités demandées
- [x] Messages privés
- [x] Liste d'utilisateurs
- [x] Commande `shut` de la part d'un utilisateur
- [ ] Envoi de fichiers
- [ ] Débogage

# Fonctionnalités supplémentaires
- [ ] Commande `kick` de la part du serveur
- [ ] Commande `help` du serveur et client
- [ ] Filtrage d'injures
