#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "message.h"

int compare(message_t message_1, message_t message_2){
    if((message_1.round < message_2.round) || (message_1.round == message_2.round && message_1.node_id <= message_2.node_id)){
        return -1;
    }
    return 1;    
}



message_queue_t new_queue(message_t message){
    message_node_t * node = (message_node_t *)malloc(sizeof(message_node_t));
    node->message = message;
    node->next = NULL;

    message_queue_t queue;
    queue.head = node;
    return queue;
}



message_t peek(message_queue_t * queue){
    return queue->head->message;
}


void pop(message_queue_t * queue){
    message_node_t * temp = queue->head;
    queue->head = queue->head->next;
    free(temp);
}


void push(message_t message, message_queue_t * queue){
    if(is_empty(queue)){
        message_node_t * node = (message_node_t *)malloc(sizeof(message_node_t));
        node->message = message;
        node->next = NULL;

        queue->head = node;
    }
    else{
        message_node_t * start = queue->head;
        
        message_node_t * node = (message_node_t *)malloc(sizeof(message_node_t));
        node->message = message;

        if(compare(message, peek(queue)) < 0){
            node->next = queue->head;
            queue->head = node;
        }
        else{
            while(start->next != NULL && compare(start->next->message, message) < 0){
                start = start->next;
            }

            node->next = start->next;
            start->next = node;
        }
    }
}


int is_empty(message_queue_t * queue){
    return (queue->head == NULL);
}





void * message_monitor_thread(void * args){

    monitor_parameters_t * monitor_params = (monitor_parameters_t *) args;
    

    int node_number = monitor_params->node_number;
    int node_id = monitor_params->node_id;
    int max_neighbour_socket = monitor_params->max_neighbour_socket;
    FILE * log_file = monitor_params->log_file;

    message_queue_t * delivery_queue = monitor_params->delivery_queue;

    fd_set socket_set;
    fd_set socket_set_tmp;
    FD_ZERO(&socket_set);
    FD_ZERO(&socket_set_tmp);

    int i;
    int ready;

    int active_sockets = 0;

    pthread_mutex_lock(monitor_params->sockets_lock);
    for(i = 0; i < node_number; i++){
        if((monitor_params->node_sockets)[i] > 0){
            FD_SET((monitor_params->node_sockets)[i], &socket_set);
            active_sockets++;
        }
    }
    pthread_mutex_unlock(monitor_params->sockets_lock);

    struct timeval timeout;

    message_t received_message;


    /*
        While there are active neighbour connection, wait for events on those sockets
    */
    while(active_sockets > 0){

        socket_set_tmp = socket_set;
        
        timeout = monitor_params->timeout;


        if((ready = select(max_neighbour_socket + 1, &socket_set_tmp, NULL, NULL, &timeout)) < 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                fprintf(log_file, "NOEUD %d : Erreur select() : %s\n", node_id, strerror(errno));
                fflush(log_file);
            #endif
            exit(1);
        }


        /*
            If more than 60s of inactivity, stop monitoring messages
        */
        if(ready == 0){
            #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                fprintf(log_file, "NOEUD %d : Inactif après %ld secondes\n", node_id, timeout.tv_sec);
                fflush(log_file);
            #endif 

            break;
        }

        int ready_socket;
        for(ready_socket = 0; ready_socket < max_neighbour_socket + 1; ready_socket++){

            if(FD_ISSET(ready_socket, &socket_set_tmp)){

                int read;
                if((read = recvTCP(ready_socket, (char *) &received_message, sizeof(received_message))) < 0){
                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_ERROR
                        fprintf(log_file, "NOEUD %d : Erreur %d réception message : %s\n", node_id, read, strerror(errno));
                        fflush(log_file);
                    #endif
                    exit(1);
                }


                /*
                    Neighbour has gracefully closed the connection, close the socket
                */
                if(read == 0){
                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                        fprintf(log_file, "NOEUD %d : Socket fermée par le pair\n", node_id);
                        fflush(log_file);
                    #endif
                    FD_CLR(ready_socket, &socket_set);
                    pthread_mutex_lock(monitor_params->sockets_lock);
                    for(i = 0; i < node_number; i++){
                        if((monitor_params->node_sockets)[i] == ready_socket){
                            (monitor_params->node_sockets)[i] = -1;
                            break;
                        }
                    }
                    close(ready_socket);
                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                        fprintf(log_file, "NOEUD %d : Socket %d fermée\n", node_id, ready_socket);
                        fflush(log_file);
                    #endif
                    pthread_mutex_unlock(monitor_params->sockets_lock);
                    continue;
                }
                
                /*
                    If the message received is a FINAL_COLOR one, this neighbour
                    will not send or receive anything anymore : deactivate the socket
                    and close it, to avoid including this neighbour in the broadcasts
                */
                if(received_message.type == MSG_COLOR_FINAL){
                    FD_CLR(ready_socket, &socket_set);
                    pthread_mutex_lock(monitor_params->sockets_lock);
                    for(i = 0; i < node_number; i++){
                        if((monitor_params->node_sockets)[i] == ready_socket){
                            (monitor_params->node_sockets)[i] = -1;
                            break;
                        }
                    }
                    close(ready_socket);
                    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_DEBUG
                        fprintf(log_file, "NOEUD %d : Socket %d fermée\n", node_id, ready_socket);
                        fflush(log_file);
                    #endif
                    pthread_mutex_unlock(monitor_params->sockets_lock);
                }

                
                /*
                    Insert the message in the priority queue, and wake up other threads
                    waiting for new messages.
                */
                
                pthread_mutex_lock(monitor_params->queue_lock);

                push(received_message, delivery_queue);

                pthread_cond_broadcast(monitor_params->queue_not_empty);
                pthread_mutex_unlock(monitor_params->queue_lock);

            }

        }
    
    }

    #if defined(LOG_LEVEL) && LOG_LEVEL >= LOG_LEVEL_INFO
        fprintf(log_file, "NOEUD %d : Plus aucune socket active, fin du thread de réception\n", node_id);
        fflush(log_file);
    #endif

    pthread_exit(NULL);

}

