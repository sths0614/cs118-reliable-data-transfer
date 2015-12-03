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

#define PACKETSIZE 960
#define PACKET_DAT 0
#define PACKET_END 1
#define PACKET_ACK 2

//creates a UDP socket
int rdt_socket(float corruptRate, float lossRate, int window);

//bind to port num
int rdt_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

//closes socket
int rdt_close(int sockfd);

//implements sender side of GBN protocol
ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

//wrapper for recv function; simulates packet loss/corruption
//assumes that buf is a least as large as maximum packet size
ssize_t lossyrecv(int sockfd, void *buf, int block, struct sockaddr *src_addr, socklen_t* addrlen);

//implements receiver side of GBN protocol. Does not return until entire message is received.
ssize_t rdt_recvfiltered(int sockfd, void *buf, const struct sockaddr *src_addr, const socklen_t addrlen);

//monolithic client function. client provides buffer containing requests and the function only exits when reply has been received. recvbuf pointer is set to point to the reply. return value is the length of received buffer.
ssize_t rdt_requestReply(int sockfd, const void* buf, size_t len, void* recvbuf, int flags, const struct sockaddr* dest_addr, socklen_t addrlen);

//receives request then sets buf pointer to request data
ssize_t rdt_waitRequest(int sockfd, void* buf);



#endif
