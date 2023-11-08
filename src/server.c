#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "tcp.h"
#include "message.h"



#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3


int main(int argc, char *argv[])
{

    if (argc != 4){
        printf("SERVEUR : utilisation : %s port_serveur fichier_graphe nb_sommets\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int node_number = atoi(argv[3]);
    char * file_path = argv[2]; 

    #if defined(LOG_LEVEL) && LOG_LEVEL > 0
        FILE * log_file = fopen("logs/server.log", "w");
    #endif

    clock_t start_time;
    start_time = clock();
    double elapsed_time = 0;


    /*
        Création, bind, listen de la socket du serveur
    */
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket == -1){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "SERVEUR : Erreur création socket : %s\n", strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons((short)port);

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "SERVEUR : Erreur nommage socket : %s\n", strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    if(listen(server_socket, node_number) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "SERVEUR : Erreur écoute socket : %s\n", strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }


    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "SERVEUR : Socket en écoute sur le port %d\n", port);
        fprintf(log_file, "SERVEUR : Attente d'identifications\n");
        fflush(log_file);
    #endif


    /*
        node_adresses : Tableau des adresses des noeuds
        node_sockets : Tableau des sockets pour communiquer avec les noeuds
    */
    struct sockaddr_in * node_addresses = (struct sockaddr_in *)malloc(node_number * sizeof(struct sockaddr_in));
    int * node_sockets = (int *)malloc(node_number * sizeof(int));
    int i;
    for(i = 0; i < node_number; i++){
        node_sockets[i] = -1;
    }

    

    int identified_nodes = 0;
    int client_socket;
    struct sockaddr_in client_address;
    socklen_t address_length = sizeof(client_address);

    int max_node_socket = 0;

    /*
        Tant que l'on n'a pas N noeuds identifiés
    */
    while (identified_nodes < node_number)
    {
       
        if((client_socket = accept(server_socket, (struct sockaddr *) &client_address, &address_length)) < 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "SERVEUR : Erreur connexion noeud : %s\n", strerror(errno));
                fflush(log_file);
            #endif
            exit(1);
        }

        message_address_t identification_message;

        if(recvTCP(client_socket, (char *) &identification_message, sizeof(identification_message)) < 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "SERVEUR : Erreur réception message identification : %s\n", strerror(errno));
                fflush(log_file);
            #endif
            exit(1);
        }

        if(identification_message.type != MSG_IDENTIFICATION){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "SERVEUR : Erreur - mauvais MSG_TYPE !\n");
                fflush(log_file);
            #endif
            exit(1);
        }

        if(node_sockets[identification_message.node_id] >= 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "SERVEUR : Erreur - noeud déjà identifié !\n");
                fflush(log_file);
            #endif         
            exit(1);
        }


        /*
            Stocker les informations du noeud
        */
        node_addresses[identification_message.node_id] = identification_message.address;
        node_sockets[identification_message.node_id] = client_socket;

        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
            fprintf(log_file, "SERVEUR : Identification avec succès du noeud %d, adresse %s:%d\n", 
                    identification_message.node_id, 
                    inet_ntoa(identification_message.address.sin_addr), 
                    ntohs(identification_message.address.sin_port)
            );
            fflush(log_file);
        #endif

        max_node_socket = (client_socket > max_node_socket) ? client_socket : max_node_socket;

        identified_nodes++;
    }
    
    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "SERVEUR : Tous les noeuds sont identifiés\n");
        fprintf(log_file, "SERVEUR : Lecture du fichier graphe\n");
        fflush(log_file);
    #endif

    FILE * graph_file;
    char * line = NULL;
    size_t line_length = 0;
    ssize_t read;

    graph_file = fopen(file_path, "r");
    if (graph_file == NULL){
        exit(1);
    }

    int node_1;
    int node_2;

    /*
        Nombre de voisins de chaque sommet dans le graphe
    */
    int * neighbour_number = (int *)malloc(node_number * sizeof(int));
    for(i = 0; i < node_number; i++){
        neighbour_number[i] = 0;
    }
    int max_degree = 0;

    while ((read = getline(&line, &line_length, graph_file)) != -1) {
        if(line[0] == 'e'){
            sscanf(line, "e %d %d\n", &node_1, &node_2);

            /*
                -1, puisque les sommets du graphe commencent à 1 dans le fichier 
            */
            int connecting_node = (node_1 < node_2) ? (node_1 - 1) : (node_2 - 1);
            int receiving_node = (node_1 < node_2) ? (node_2 - 1) : (node_1 - 1);

            neighbour_number[connecting_node]++;
            neighbour_number[receiving_node]++;

            if(neighbour_number[connecting_node] > max_degree || neighbour_number[receiving_node] > max_degree){
                max_degree++;
            }

            message_address_t connection_message;
            connection_message.type = MSG_CONNECT;
            connection_message.node_id = receiving_node;
            connection_message.address = node_addresses[receiving_node];

            if(sendTCP(node_sockets[connecting_node], (char *)&connection_message, sizeof(connection_message)) < 0){
                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                    fprintf(log_file, "SERVEUR : Erreur envoi ordre connexion : %s\n", strerror(errno));
                    fflush(log_file);
                #endif
                exit(1);
            }

            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                fprintf(log_file, "SERVEUR : Ordre connexion du noeud %d à %d\n", connecting_node, receiving_node);
                fflush(log_file);
            #endif

        }
    }

    fclose(graph_file);

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "SERVEUR : Fin de la lecture du graphe\n");
        fprintf(log_file, "SERVEUR : Envoi de END_CONNECT à tous les noeuds\n");
        fprintf(log_file, "SERVEUR : Degré max : %d\n", max_degree);
        fflush(log_file);
    #endif

    message_address_t end_connection_message;
    end_connection_message.type = MSG_END_CONNECT;
    end_connection_message.color = max_degree;

    for(i = 0; i < node_number; i++){
        end_connection_message.node_id = neighbour_number[i];

        if(sendTCP(node_sockets[i], (char *)&end_connection_message, sizeof(end_connection_message)) < 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "SERVEUR : Erreur envoi fin de connexion : %s\n", strerror(errno));
                fflush(log_file);
            #endif
            exit(1);
        }
    }

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "SERVEUR : Mise en place du réseau terminée\n");
        fflush(log_file);
    #endif


    elapsed_time = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "SERVEUR : Terminaison (%.5fs)\n", elapsed_time);
        fflush(log_file);
    #endif


    for(i = 0; i < node_number; i++){
        close(node_sockets[i]);
    }

    close(server_socket);

    free(node_addresses);
    free(node_sockets);
    //free(node_colors);
    free(neighbour_number);

    #if defined(LOG_LEVEL) && LOG_LEVEL > 0
        fclose(log_file);
    #endif

    return 0;

}