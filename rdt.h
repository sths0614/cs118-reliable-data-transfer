#ifndef RDT
#define RDT

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//checks if two addresses are equal
int rdt_checkAddrMatch(const struct sockaddr* dest_addrA, const struct sockaddr* dest_addrB, socklen_t* addrlenA, socklen_t* addrlenB);

//creates a UDP socket
int rdt_socket(float corruptRate, float lossRate, ssize_t window, long timeout, long connTimeout);


//bind to port num
int rdt_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

//closes socket
int rdt_close(int sockfd);

//implements sender side of GBN protocol
ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, const struct sockaddr *dest_addr, socklen_t addrlen);

//wrapper for recv function; simulates packet loss/corruption
//buf must be at least as large as maximum packet size
ssize_t lossyrecv(int sockfd, void *buf, int block, struct sockaddr *src_addr, socklen_t* addrlen);

//implements receiver side of GBN protocol. Does not return until entire message is received.
//the function will only accept packets from the address specified in src_addr and addrlen
//if addrlen points to a value of 0, the function will accept packets from any address; the function will lock to the address of the first data packet it receives
//void** buf is essentially a reference to a pointer variable. The function will change the pointer variable to point at the buffer containing the received message.
ssize_t rdt_recvfrom(int sockfd, void **buf, struct sockaddr *src_addr, socklen_t *addrlen);

//monolithic client function. client provides buffer containing requests and the function only exits when reply has been received. recvbuf pointer is set to point to the reply. return value is the length of received buffer.
ssize_t rdt_requestReply(int sockfd, const void* buf, size_t len, void** recvbuf, struct sockaddr* dest_addr, socklen_t addrlen);



#endif
