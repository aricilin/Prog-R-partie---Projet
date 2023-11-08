#include "tcp.h"
#include <stdio.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int sendTCP(int sock, char * message, int messageSize){
    int bytesLeft = messageSize;
    int bytesSent = 0;
    int totalBytesSent = 0;
    while(bytesLeft > 0){
        if ((bytesSent = send(sock, message, bytesLeft, 0)) < 0){
            //perror("Erreur send");
            //exit(1);
            return -errno;
        }
        message += bytesSent;
        bytesLeft -= bytesSent;
        totalBytesSent += bytesSent;
    }
    if(bytesLeft > 0){
        return -1;
    }
    return totalBytesSent; // succès
}


int recvTCP(int sock, char * message, int messageSize){
    int bytesLeft = messageSize;
    int bytesRead = 0;
    int totalBytesRead = 0;
    while(bytesLeft > 0){
        if((bytesRead = recv(sock, message, bytesLeft, 0)) < 0){
            //perror("Erreur recv");
            //exit(1);
            return -errno;
        }
        if(bytesRead == 0){
            return 0;
        }
        message += bytesRead;
        bytesLeft -= bytesRead;
        totalBytesRead += bytesRead;
    }
    if(bytesLeft > 0){
        return -1;
    }
    return totalBytesRead; // succès
}