#!/bin/bash


server_ip=$1
server_port=$2
node_number=$3
coloring_file=$4

mkdir -p logs
rm -f $coloring_file

> node_pids
# Lancer les N processus noeuds
for (( i=0; i<=$(($node_number-1)); i++ ))
do
  ./bin/node $server_ip $server_port $i $node_number $coloring_file & 
  echo $! >> node_pids
done