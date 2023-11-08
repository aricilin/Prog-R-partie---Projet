#!/bin/bash



# Paramètres
graph_file=$1
server_port=$2

# Récupérer le nombre de sommets du graphe  
node_number=$(grep -E '^p (edge|col)' $graph_file | cut -d " " -f 3)

mkdir -p logs

# Lancer le serveur
./bin/server $server_port $graph_file $node_number & 
echo $! > server_pid