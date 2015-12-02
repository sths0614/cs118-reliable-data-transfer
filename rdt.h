#ifndef RDT
#define RDT

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

extern int CORRUPTRATE;
extern int LOSSRATE;

//creates a UDP socket
int rdt_socket();

//implements sender side of GBN protocol
ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

//wrapper for recv function; separates received data into packets and then simulates packet loss/corruption
//timeout should be based on poll function
ssize_t lossyrecv(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t* addrlen);

//implements receiver side of GBN protocol. Does not return until entire message is received.
ssize_t rdt_recvfrom(int sockfd, void *buf, int flags, struct sockaddr *src_addr, socklen_t* addrlen);

//monolithic client function. client provides buffer containing requests and the function only exits when reply has been received. recvbuf pointer is set to point to the reply. return value is the length of received buffer.
ssize_t rdt_requestReply(int sockfd, const void* buf, size_t len, void* recvbuf, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);

//receives request then sets buf pointer to request data
ssize_t rdt_waitRequest(int sockfd, void* buf);



#endif
