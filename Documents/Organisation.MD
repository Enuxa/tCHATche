# Bugs constatés
- Après l'envoi d'un fichier volumineux (plusieurs Mo), l'entrée standard du serveur n'est pas vide, même si l'utilisateur n'a rien saisi
- Si un client est déconnecté, le serveur crashe. Correction possible : vérifier que `errno` vaut `EPIPE`, déconnecter l'utilisateur et ignorer le signal `SIGPIPE`

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
- [x] Envoi de fichiers
- [x] Débogage

# Fonctionnalités supplémentaires
- [x] Commande `help` du serveur et client
