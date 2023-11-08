#ifndef MESSAGE_H
#define MESSAGE_H

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "tcp.h"

#define MSG_IDENTIFICATION 100
#define MSG_CONNECT 101
#define MSG_END_CONNECT 102
#define MSG_COLOR 103
#define MSG_COLOR_FINAL 104
#define MSG_END_ROUND 105


#ifndef LOG_LEVEL_ERROR

#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3

#endif

/*
    Message object, with network address
*/
typedef struct message_address {
    int type;
    int node_id;
    int color;
    int round;
    struct sockaddr_in address;
} message_address_t;

/*
    Message object, without network address
*/
typedef struct message {
    int type;
    int node_id;
    int color;
    int round;
} message_t;


/*
    Message block in the message priority queue
*/
typedef struct message_node {
    message_t message;
    struct message_node * next;
} message_node_t;



/*
    Message priority queue
    Priority is based on lowest round number, and lowest ID if equal
*/
typedef struct message_queue {
    message_node_t * head;
} message_queue_t;



/*
    Monitoring thread parameters
     - delivery_queue : Priority queue to store received messages
     - node_sockets : Socket array to communicate with neighbours
     - max_neighbour_socket : Max file descriptor of a neighbour socket
     - node_number : total number of vertices in the graph
     - node_id : ID of self

*/
typedef struct monitor_parameters {
    message_queue_t * delivery_queue;
    int * node_sockets;
    int max_neighbour_socket;
    int node_number;
    int node_id;
    struct timeval timeout;
    FILE * log_file;

    pthread_mutex_t * queue_lock;
    pthread_mutex_t * sockets_lock;
    pthread_cond_t * queue_not_empty;
    pthread_cond_t * queue_empty;
} monitor_parameters_t;


/*
    Returns < 0 if message_1 has higher priority than message_2,
    0 if equal priority, and else returns > 0.
*/
int compare(message_t message_1, message_t message_2);


/*
    Create a new queue from message
*/
message_queue_t new_queue(message_t message);


/*
    Returns the message with highest priority in queue
    (does not remove it from the queue)
*/
message_t peek(message_queue_t * queue);

/*
    Remove message with highest priority from queue
*/
void pop(message_queue_t * queue);

/*
    Insert a message in the queue (at adequate place for its priority)
*/
void push(message_t message, message_queue_t * queue);

/*
    Returns > 0 if queue is empty, else returns 0 
*/
int is_empty(message_queue_t * queue);



/*
    Monitor thread entrypoint
*/
void * message_monitor_thread(void * parameters);


#endif