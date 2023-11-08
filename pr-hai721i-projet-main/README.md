# Programmation Répartie - Projet : Coloration de Sommets


Implémentation d'algorithmes de coloration de graphes dans un système distribué.


## Installation et usage

### Installation/compilation

Depuis Gitlab :
```
git clone https://gitlab.com/loic.allegre/pr-hai721i-projet.git
cd pr-hai721i-projet
make
```

Avec le code source :
```
cd <dossier_du_projet>
make
```

### Usage

Pour lancer tous les processus d'un coup (serveur et noeuds) :
```
./run_project.sh <fichier_graphe> <port_serveur> <fichier_coloration>
```

Pour vérifier la coloration et compter le nombre de couleurs :
```
./bin/check-coloration <fichier_graphe> <fichier_coloration>
```


### Arrêt d'urgence

En cas de problème, les processus sont censés s'arrêter automatiquement après 60s d'inactivité. Si ce n'est pas le cas, pour tuer tous les processus lancés, dans le cas où ils ne se seraient pas arrêtés correctement (en cas de bug seulement):
```
./cleanup node_pids server_pid
```

### Logs

Pour avoir les logs d'exécution des différents processus :
- Dans le `Makefile`, mettre la variable `LOG_LEVEL` au niveau souhaité, et recompiler (`make clean && make`)
- Les scripts bash de lancement créent automatiquement le dossier `./logs/`.
- Si les processus sont lancés à la main, il faut créer les dossiers `logs` au préalable. 
- Les logs seront écrits dans `logs/server.log` et `logs/node_i.log`
- Pour surveiller les logs en temps réel : `tail -F <fichier_log>`

