#ifndef TCP_H
#define TCP_H


/*
    Send a message of size messageSize through sock
*/
int sendTCP(int sock, char * message, int messageSize);

/*
    Receive a message of size messageSize from sock
*/
int recvTCP(int sock, char * message, int messageSize);


#endif