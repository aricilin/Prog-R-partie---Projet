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
#include <time.h>
#include <string.h>
#include "tcp.h"
#include "message.h"
#include "random_color.h"


#ifndef LOG_LEVEL_ERROR

#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

#endif

#define IDLE_TIMEOUT_S 60
#define IDLE_TIMEOUT_US 0


int main(int argc, char *argv[])
{

    if (argc != 6){
        printf("NOEUD : utilisation : %s ip_serveur port_serveur id_noeud nb_sommets fichier_coloration\n", argv[0]);
        exit(1);
    }

    char * server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int node_id = atoi(argv[3]);
    int node_number = atoi(argv[4]);
    char * coloring_path = argv[5];


    #if defined(LOG_LEVEL) && LOG_LEVEL > 0
        char log_filename[50];
        sprintf(log_filename, "logs/node_%d.log", node_id);
        FILE * log_file = fopen(log_filename, "w");
    #endif

    FILE * coloring_file = fopen(coloring_path, "a");
    setvbuf(coloring_file, NULL, _IOLBF, 1024);

    /*
        node_sockets : Sockets for neighbour communications
    */
    int * node_sockets = (int *)malloc(node_number * sizeof(int));
    int i;
    for(i = 0; i < node_number; i++){
        node_sockets[i] = -1;
    }


    /*
        Socket for server communication
    */
    int server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket == -1){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur création socket serveur : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    /*
        Listening socket, for incoming connection requests
    */
    int listening_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (listening_socket == -1){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur création socket écoute : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    /*
        Connect to server
    */
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(server_ip);
    server_address.sin_port = htons((short)server_port);
    
    if(connect(server_socket, (struct sockaddr *)&server_address, sizeof(struct sockaddr_in)) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur connexion serveur : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    /*
        Using server_socket, get one IP adress
    */
    struct sockaddr_in self_address;
    socklen_t self_address_size = sizeof(self_address);
    getsockname(server_socket, (struct sockaddr *) &self_address, &self_address_size);


    /*
        Bind to this IP address, with random port
    */
    self_address.sin_family = AF_INET;
    self_address.sin_port = 0;

    if (bind(listening_socket, (struct sockaddr *) &self_address, sizeof(self_address)) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur nommage socket : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    /*
        Get assigned port
    */
    getsockname(listening_socket, (struct sockaddr *) &self_address, &self_address_size);

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "NOEUD %d: Socket nommée à l'adresse %s:%d\n", node_id, inet_ntoa(self_address.sin_addr), ntohs(self_address.sin_port));
        fflush(log_file);
    #endif

    if(listen(listening_socket, node_number + 1) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur mise en écoute : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    /*
        Send IP and port to server
    */
    message_address_t identification_message;
    identification_message.type = MSG_IDENTIFICATION;
    identification_message.node_id = node_id;
    identification_message.address = self_address;

    if(sendTCP(server_socket, (char *)&identification_message, sizeof(identification_message)) < 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur envoi identification : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
        exit(1);
    }

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "NOEUD %d : Identification envoyée\n", node_id);
        fprintf(log_file, "NOEUD %d : Attente d'ordres de connexion\n", node_id);
        fflush(log_file);
    #endif

    message_address_t connection_message;
    message_t incoming_connection_message;

    int connected_neighbours = 0;
    int expected_neigbours = node_number;
    int max_degree = 0;
    
    fd_set socket_set;
    fd_set socket_set_tmp;
    FD_ZERO(&socket_set);
    FD_ZERO(&socket_set_tmp);

    FD_SET(listening_socket, &socket_set);
    FD_SET(server_socket, &socket_set);

    int max_socket_descriptor = (listening_socket > server_socket) ? listening_socket : server_socket;
    int ready;
    int max_neighbour_socket = 0;

    struct timeval timeout;
    
    while(1){

        socket_set_tmp = socket_set;
        
        timeout.tv_sec = IDLE_TIMEOUT_S;
        timeout.tv_usec = IDLE_TIMEOUT_US;

        if((ready = select(max_socket_descriptor + 1, &socket_set_tmp, NULL, NULL, &timeout)) < 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "NOEUD %d : Erreur select() : %s\n", node_id, strerror(errno));
                fflush(log_file);
            #endif
            exit(1);
        }

        /*
            select() timeout, exit loop
        */
        if(ready == 0){
           #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                fprintf(log_file, "NOEUD %d : Inactif après %ld secondes\n", node_id, timeout.tv_sec);
                fflush(log_file);
            #endif 

            break;
        }

        int ready_socket;
        for(ready_socket = 0; ready_socket < max_socket_descriptor + 1; ready_socket++){

            if(FD_ISSET(ready_socket, &socket_set_tmp)){
                
                /*
                    If receiving a connection request
                */
                if(ready_socket == listening_socket){
                    

                    int incoming_socket;

                    if((incoming_socket = accept(listening_socket, NULL, NULL)) < 0){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                            fprintf(log_file, "NOEUD %d : Erreur accept connexion voisin : %s\n", node_id, strerror(errno));
                            fflush(log_file);
                        #endif
                        exit(1);
                    }

                    int read;
                    if((read = recvTCP(incoming_socket, (char *) &incoming_connection_message, sizeof(incoming_connection_message))) < 0){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                            fprintf(log_file, "NOEUD %d : Erreur réception identification : %s\n", node_id, strerror(errno));
                            fflush(log_file);
                        #endif
                        exit(1);
                    }

                    if(read == 0){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                            fprintf(log_file, "NOEUD %d : Socket fermée par le pair\n", node_id);
                            fflush(log_file);
                        #endif
                        FD_CLR(incoming_socket, &socket_set);
                        close(incoming_socket);
                        continue;
                    }

                    if(incoming_connection_message.type != MSG_IDENTIFICATION){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                            fprintf(log_file, "NOEUD %d: Erreur - mauvais MSG_TYPE !\n", node_id);
                            fflush(log_file);
                        #endif
                        exit(1);
                    }

                    node_sockets[incoming_connection_message.node_id] = incoming_socket;
                    max_neighbour_socket = (incoming_socket > max_neighbour_socket) ? incoming_socket : max_neighbour_socket;

                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                        fprintf(log_file, "NOEUD %d: Accepté une connexion du noeud %d\n", node_id, incoming_connection_message.node_id);
                        fflush(log_file);
                    #endif

                    connected_neighbours++;

                    continue;

                }

                /*
                    If receiving a message from server
                */
                if(ready_socket == server_socket){

                    int read;
                    if((read = recvTCP(ready_socket, (char *) &connection_message, sizeof(connection_message))) < 0){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                            fprintf(log_file, "NOEUD %d : Erreur réception ordre connexion : %s\n", node_id, strerror(errno));
                            fflush(log_file);
                        #endif
                        exit(1);
                    }

                    if(read == 0){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                            fprintf(log_file, "NOEUD %d : Socket fermée par le serveur (%d, %d)\n", node_id, ready_socket, server_socket);
                            fflush(log_file);
                        #endif
                        FD_CLR(server_socket, &socket_set);
                        shutdown(server_socket, SHUT_RDWR);
                        //close(server_socket);
                        continue;
                    }

                    if(connection_message.type != MSG_CONNECT && connection_message.type != MSG_END_CONNECT){
                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                            fprintf(log_file, "NOEUD %d : Erreur, mauvais MSG_TYPE !\n", node_id);
                            fflush(log_file);
                        #endif
                        exit(1);
                    }

                    /*
                        Case 1 : END_CONNECT
                        Server has sent all connection orders
                        Receive self degree (number of neighbours to expect)
                    */
                    if(connection_message.type == MSG_END_CONNECT){
                        expected_neigbours = connection_message.node_id;
                        max_degree = connection_message.color;

                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                            fprintf(log_file, "NOEUD %d : Recu END_CONNECT, %d voisins attendus, degré max %d\n", node_id, expected_neigbours, max_degree);
                            fflush(log_file);
                        #endif

                        continue;
                    }
                    

                    /*
                        Cas 2 : CONNECT
                        Order from server to connect to neighbour at given network address
                    */
                    if(connection_message.type == MSG_CONNECT){

                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                            fprintf(log_file, "NOEUD %d: Reçu ordre de connexion de %d à %d, adresse %s:%d\n", 
                                node_id,
                                node_id,
                                connection_message.node_id,
                                inet_ntoa(connection_message.address.sin_addr),
                                ntohs(connection_message.address.sin_port)
                            );
                            fflush(log_file);
                        #endif

                        int neighbour_socket = socket(PF_INET, SOCK_STREAM, 0);
                        if (neighbour_socket == -1){
                            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                                fprintf(log_file, "NOEUD %d : Erreur création socket voisin : %s\n", node_id, strerror(errno));
                                fflush(log_file);
                            #endif
                            exit(1);
                        }

                        if(connect(neighbour_socket, (struct sockaddr *)&connection_message.address, sizeof(struct sockaddr_in)) < 0){
                            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                                fprintf(log_file, "NOEUD %d : Erreur connexion voisin : %s\n", node_id, strerror(errno));
                                fflush(log_file);
                            #endif
                            exit(1);
                        }

                        message_t neigbour_message;
                        neigbour_message.type = MSG_IDENTIFICATION;
                        neigbour_message.node_id = node_id;


                        if(sendTCP(neighbour_socket, (char *)&neigbour_message, sizeof(neigbour_message)) < 0){
                            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                                fprintf(log_file, "NOEUD %d : Erreur identification au voisin : %s\n", node_id, strerror(errno));
                                fflush(log_file);
                            #endif
                            exit(1);
                        }

                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                            fprintf(log_file, "NOEUD %d: Connecté au voisin %d\n", node_id, connection_message.node_id);
                            fflush(log_file);
                        #endif

                        node_sockets[connection_message.node_id] = neighbour_socket;
                        max_neighbour_socket = (neighbour_socket > max_neighbour_socket) ? neighbour_socket : max_neighbour_socket;
                        connected_neighbours++;

                        continue;

                    }

                }

            }

        }

        /*
            Connection established with all neighbours
        */
        if(connected_neighbours >= expected_neigbours){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                fprintf(log_file, "NOEUD %d : Connecté à %d/%d voisins\n", node_id, connected_neighbours, expected_neigbours);
                fflush(log_file);
            #endif 

            break;
        }

    }

    close(server_socket);


    /*
        Start of the coloring algorithme
    */

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
        fprintf(log_file, "NOEUD %d : Début coloration\n", node_id);
        fflush(log_file);
    #endif

    unsigned int seed;
    FILE * random_file = fopen("/dev/random", "r");
    fread(&seed, sizeof(seed), 1, random_file);
    fclose(random_file);
    srand(seed);

    clock_t start_time;
    start_time = clock();
    double elapsed_time = 0;

    int node_color = node_id;
    int final_color = 0;
    int round_number = 0;
    int neighbour;
    int active_neighbours = connected_neighbours;
    int degree = connected_neighbours;

    int * neighbours_final_colors = (int *)malloc((node_number + 1) * sizeof(int));
    neighbours_final_colors[0] = 1;
    for(i = 1; i < node_number + 1; i++){
        neighbours_final_colors[i] = 0;
    }

    int * neighbours_temporary_colors = (int *)malloc((node_number + 1) * sizeof(int));
    neighbours_final_colors[0] = 1;

 
    FD_ZERO(&socket_set);
    FD_ZERO(&socket_set_tmp);

    for(i = 0; i < node_number; i++){
        if(node_sockets[i] > 0){
            FD_SET(node_sockets[i], &socket_set);
        }
    }


    pthread_mutex_t sockets_lock;
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_empty;
    pthread_cond_t queue_not_empty;

    pthread_mutex_init(&sockets_lock, NULL);
    pthread_mutex_init(&queue_lock, NULL);
    pthread_cond_init(&queue_empty, NULL);
    pthread_cond_init(&queue_not_empty, NULL);

    message_queue_t delivery_queue;
    delivery_queue.head = NULL;

    monitor_parameters_t monitor_params;
    monitor_params.node_number = node_number;
    monitor_params.node_id = node_id;
    monitor_params.max_neighbour_socket = max_neighbour_socket;
    monitor_params.node_sockets = node_sockets;
    monitor_params.delivery_queue = &delivery_queue;

    timeout.tv_sec = IDLE_TIMEOUT_S;
    timeout.tv_usec = IDLE_TIMEOUT_US;
    monitor_params.timeout = timeout;

    monitor_params.log_file = NULL;
    #if defined(LOG_LEVEL) && LOG_LEVEL > 0
        monitor_params.log_file = log_file;
    #endif

    monitor_params.sockets_lock = &sockets_lock;
    monitor_params.queue_lock = &queue_lock;
    monitor_params.queue_empty = &queue_empty;
    monitor_params.queue_not_empty = &queue_not_empty;


    pthread_t reception_thread;

    /*
        Start message monitoring thread
    */
    if(pthread_create(&reception_thread, NULL, message_monitor_thread, &monitor_params) != 0){
        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
            fprintf(log_file, "NOEUD %d : Erreur création thread réception : %s\n", node_id, strerror(errno));
            fflush(log_file);
        #endif
    };


    /*
        Start the algorithm
        1 iteration = 1 round
        Set timeout to limit execution time at 60s
    */
    while(elapsed_time < 60){

        round_number++;

        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
            fprintf(log_file, "---------------------------------------------\n");
            fprintf(log_file, "NOEUD %d : Début round %d, voisins actifs : %d\n", node_id, round_number, active_neighbours);
            fflush(log_file);
        #endif


        /*
            With probability 1/2, choose a non-zero color.
            Non-zero color is chosen uniformly at random from palette [1, degree + 1], 
            excluding color permanently taken by neighbours (those in neighbours_final_colors)
        */

        for(i = 1; i < node_number + 1; i++){
            neighbours_temporary_colors[i] = 0;
        }

        node_color = random_int(2);

        if(node_color == 0){

            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                fprintf(log_file, "NOEUD %d : Tour à vide\n", node_id);
                fflush(log_file);
            #endif

        }
        else{ 
            
            node_color = random_color(degree + 1, neighbours_final_colors);

            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                fprintf(log_file, "NOEUD %d : Couleur choisie : %d\n", node_id, node_color);
                fflush(log_file);
            #endif

        }
       

        /*
            Broadcast the chosen color to all active neighbours
            Color is broadcast even if it is zero.
        */

        message_t color_message;
        color_message.type = MSG_COLOR;
        color_message.node_id = node_id;
        color_message.color = node_color;
        color_message.round = round_number;


        pthread_mutex_lock(&sockets_lock);
        for(neighbour = 0; neighbour < node_number; neighbour++){
            if(node_sockets[neighbour] > 0){
                if(sendTCP(node_sockets[neighbour], (char *)&color_message, sizeof(color_message)) < 0){
                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                        fprintf(log_file, "NOEUD %d : Erreur envoi message COLOR (socket %d) : %s\n", node_id, node_sockets[neighbour], strerror(errno));
                        fflush(log_file);
                    #endif
                    exit(1);
                }
            }
        }
        pthread_mutex_unlock(&sockets_lock);

        int colors_received = 0;
        int colors_expected = active_neighbours;
        int ready_neighbours = 0;

        message_t received_message;


        /*
            Handle message from neighbours
            Messages are handled only if their round_number is at most equal to self's round number,
            otherwise they are left in the queue

            For each round, we expect exactly two messages from each active neighbour :
            - One color message
            - One end message, either END_ROUND (neighbour is ready to continue to next round), 
              or FINAL_COLOR (neighbour has chosen a permanent color, and stops being active)
        */
        while((ready_neighbours < active_neighbours) || (colors_received < colors_expected)){

            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                fprintf(log_file, "NOEUD %d : %d voisins actifs, %d ready, %d couleurs recues\n", node_id, active_neighbours, ready_neighbours, colors_received);
                fflush(log_file);
            #endif 


            pthread_mutex_lock(&queue_lock);

            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                fprintf(log_file, "NOEUD %d : Pris le mutex sur la queue, queue vide : %d\n", node_id, is_empty(&delivery_queue));
                fflush(log_file);
            #endif 

            /*
                Check the queue for new data :
                If it is empty or if it has messages from future rounds, wait to be waken up by monitor thread
                Else, pop the highest priority message and handle it
            */
            while(is_empty(&delivery_queue) || ((!is_empty(&delivery_queue)) && peek(&delivery_queue).round > round_number)){
                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                    fprintf(log_file, "NOEUD %d : En attente de messages\n", node_id);
                    fflush(log_file);
                #endif 
                pthread_cond_wait(&queue_not_empty, &queue_lock);
                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                    fprintf(log_file, "NOEUD %d : Réveillé par le signal\n", node_id);
                    fflush(log_file);
                #endif
            }

            
            /*
                There is a message to handle
            */
            
            received_message = peek(&delivery_queue);
            
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                fprintf(log_file, "NOEUD %d : Désenfilé un message du round %d\n", node_id, received_message.round);
                fflush(log_file);
            #endif 
            
            pop(&delivery_queue);

            pthread_mutex_unlock(&queue_lock);


            /*
                If it is a color message, store the temporary color.
                
                If we have all neighbour color, check for conflict with self's chosen color
                If there is no conflict and our color is not zero, we can set our color as permanent, 
                notify all neighbours of our choice, and exit the loop (i.e. stop being active).

                Else (either color is zero or there are conflicts), we notify all neighbours that we are ready
                to continue to the next round, and continue waiting for END_ROUND or FINAL_COLOR messages
            */
            if(received_message.type == MSG_COLOR){
                        
                neighbours_temporary_colors[received_message.color] = 1;
                colors_received++;

                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                    fprintf(log_file, "NOEUD %d : Recu couleur %d depuis %d\n", node_id, received_message.color, received_message.node_id);
                    fflush(log_file);
                #endif

                if(colors_received >= colors_expected){
                    if(node_color > 0 && neighbours_final_colors[node_color] == 0 && neighbours_temporary_colors[node_color] == 0){
                        
                        final_color = node_color;

                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
                            fprintf(log_file, "NOEUD %d : Trouvé couleur finale %d, envoi FINAL\n", node_id, final_color);
                            fflush(log_file);
                        #endif

                        message_t final_color_message;
                        final_color_message.type = MSG_COLOR_FINAL;
                        final_color_message.node_id = node_id;
                        final_color_message.color = final_color;
                        final_color_message.round = round_number;

                        pthread_mutex_lock(&sockets_lock);
                        for(neighbour = 0; neighbour < node_number; neighbour++){
                            if(node_sockets[neighbour] > 0){
                                if(sendTCP(node_sockets[neighbour], (char *)&final_color_message, sizeof(final_color_message)) < 0){
                                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                                        fprintf(log_file, "NOEUD %d : Erreur envoi message FINAL_COLOR : %s\n", node_id, strerror(errno));
                                        fflush(log_file);
                                    #endif
                                    exit(1);
                                }
                            }
                        }
                        pthread_mutex_unlock(&sockets_lock);
                        

                        elapsed_time = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
                        fprintf(coloring_file, "%d %d %.5f %d\n", node_id + 1, final_color, elapsed_time, round_number);

                        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                            fprintf(log_file, "NOEUD %d : Sortie de boucle\n", node_id);
                            fflush(log_file);
                        #endif

                        break;

                    }
                    else{
                        message_t end_round_message;
                        end_round_message.type = MSG_END_ROUND;
                        end_round_message.node_id = node_id;
                        end_round_message.color = node_color;
                        end_round_message.round = round_number;

                        pthread_mutex_lock(&sockets_lock);
                        for(neighbour = 0; neighbour < node_number; neighbour++){
                            if(node_sockets[neighbour] > 0){
                                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                                    fprintf(log_file, "NOEUD %d : Envoi END_ROUND %d à %d\n", node_id, round_number, neighbour);
                                    fflush(log_file);
                                #endif
                                if(sendTCP(node_sockets[neighbour], (char *)&end_round_message, sizeof(end_round_message)) < 0){
                                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                                        fprintf(log_file, "NOEUD %d : Erreur envoi message END_ROUND : %s\n", node_id, strerror(errno));
                                        fflush(log_file);
                                    #endif
                                    exit(1);
                                }
                            }
                        }
                        pthread_mutex_unlock(&sockets_lock);
                    }
                }
                continue;
            }



            /*
                A neighbour has chosen a permanent color and deactivated
                Store the final color, and deactivate the corresponding socket
                (deactivate it only : the task of actually closing the socket is left
                to the monitoring thread)
            */
            if(received_message.type == MSG_COLOR_FINAL){

                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                    fprintf(log_file, "NOEUD %d : Recu FINAL %d depuis %d\n", node_id, received_message.color, received_message.node_id);
                    fflush(log_file);
                #endif
                
                neighbours_final_colors[received_message.color] = 1;
                active_neighbours--;
                pthread_mutex_lock(&sockets_lock);
                //close(ready_socket);
                node_sockets[received_message.node_id] = -1;
                pthread_mutex_unlock(&sockets_lock);
                continue;
            }

            /*
                A neighbour is ready for the next round
                We can continue to the next round when we have as many ready neighbours as active ones,
                i.e. when each active neighbour has sent us either END_ROUND or FINAL_COLOR.
            */
            if(received_message.type == MSG_END_ROUND){

                #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                    fprintf(log_file, "NOEUD %d : Recu END_ROUND %d depuis %d\n", node_id, received_message.color, received_message.node_id);
                    fflush(log_file);
                #endif
                
                ready_neighbours++;
                continue;
            }

        }

        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
            fprintf(log_file, "NOEUD %d : Sortie boucle, ready %d / active %d, received %d / expected %d\n", node_id, ready_neighbours, active_neighbours, colors_received, colors_expected);
            fflush(log_file);
        #endif
        

        /*
            Special case : we have no active neighbours left
            In that case, if our color is suitable (non-zero and non conflicting),
            we can choose is as permanent and deactivate.
        */
        if(active_neighbours == 0 && final_color == 0 && node_color > 0){
            if(neighbours_final_colors[node_color] == 0 && neighbours_temporary_colors[node_color] == 0){
                final_color = node_color;

                elapsed_time = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
                fprintf(coloring_file, "%d %d %.5f %d\n", node_id + 1, final_color, elapsed_time, round_number);
            }
        }
        
        elapsed_time = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;

        if(final_color > 0){
            break;
        }

        #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
            fprintf(log_file, "NOEUD %d : Fin du round %d : %d couleurs, %d actifs, %d ready\n", node_id, round_number, colors_received, active_neighbours, ready_neighbours);
            fflush(log_file);
        #endif


    }


    /*
        In the event that we exited the loop without finding a permanent color
        (for example, if the algorithm has timed out or an error has occurred), 
        we take 0 as final color : it is not a valid color, the subsequent coloring will be incomplete
    */
    if(final_color == 0){

        elapsed_time = ((double)(clock() - start_time)) / CLOCKS_PER_SEC;
        fprintf(coloring_file, "%d %d %.5f %d\n", node_id + 1, final_color, elapsed_time, round_number);

    }


    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "NOEUD %d : Travail terminé\n", node_id);
        fprintf(log_file, "NOEUD %d : Couleur finale : %d\n", node_id, final_color);
        fprintf(log_file, "NOEUD %d : Trouvée en %d rounds, en %f secondes\n", node_id, round_number, elapsed_time);
        fprintf(log_file, "NOEUD %d : Terminaison\n", node_id);
        fflush(log_file);
    #endif

    sleep(5);

    close(listening_socket);
    for(i = 0; i < node_number; i++){
        close(node_sockets[i]);
    }
    
    free(node_sockets);
    free(neighbours_temporary_colors);
    free(neighbours_final_colors);

    while(!is_empty(&delivery_queue)){
        pop(&delivery_queue);
    }

    #if defined(LOG_LEVEL) && LOG_LEVEL > 0
        fclose(log_file);
    #endif

    return 0;

}