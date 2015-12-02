#include "rdt.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

static char recvBuffer[2000];

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int rdt_socket()
{
     return socket(AF_INET, SOCK_DGRAM, 0);
}

int rdt_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
     int result;
     result = bind(sockfd, addr, addrlen);
     if (result) error("rdt_connect: bind error");
     
}
