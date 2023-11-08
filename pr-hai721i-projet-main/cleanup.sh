#!/bin/bash


# Dans le cas (qui normalement n'arrive pas) où les processus ne quittent
# pas correctement, leurs PIDs sont stockés dans 2 fichiers, et on peut
# les arrêter avec ce script :
#
# ./cleanup.sh node_pids server_pid
#

pids_file=$1
pid_server=$2

while read pid; do
    kill "$pid"
done < "$pids_file" 

if [ ! -z "${pid_server}" ]
then
    kill $(cat $pid_server)
fi