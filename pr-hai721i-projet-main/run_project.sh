#!/bin/bash



# Paramètres
graph_file=$1
server_port=$2
coloring_file=$3

# Récupérer le nombre de sommets du graphe  
node_number=$(grep -E '^p (edge|col)' $graph_file | cut -d " " -f 3)

# Récupérer l'IP (v4) de la première interface non-localhost trouvée
server_ip=$(ip a | grep -Eo 'inet (addr:)?([0-9]*\.){3}[0-9]*' | grep -Eo '([0-9]*\.){3}[0-9]*' | grep -v '127.0.0.1' | grep -m 1 -Eo '([0-9]*\.){3}[0-9]*')

mkdir -p logs
rm -f $coloring_file

# Lancer le serveur
./bin/server $server_port $graph_file $node_number & 
echo $! > server_pid

sleep 0.1

>node_pids
# Lancer les N processus noeuds
for (( i=0; i<=$(($node_number-1)); i++ ))
do
  ./bin/node $server_ip $server_port $i $node_number $coloring_file & 
  echo $! >> node_pids
done