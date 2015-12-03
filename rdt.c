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
#include <errno.h>

#define PACKETSIZE 1000;
static char rdt_recvBuffer[2000];
static char* rdt_sendBuffer;
static ssize_t rdt_sendBufferSize;

static float rdt_corruptRate;
static float rdt_lossRate;
static ssize_t rdt_window;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int rdt_socket(float corruptRate, float lossRate, ssize_t window)
{
     time_t t;
     srand((unsigned) time(&t));
     
     if (corruptRate >= 0)
          rdt_corruptRate = corruptRate;
     if (corruptRate >= 0)
          rdt_lossRate = lossRate;
     if (window > 0)
          rdt_window = window;
     int sock = socket(AF_INET, SOCK_DGRAM, 0);
     fcntl(sock, F_SETFL, O_NONBLOCK);
     return sock;
}

int rdt_close(int sockfd);
{
     return close(sockfd);
}

ssize_t lossyrecv(int sockfd, void *buf, int block, struct sockaddr *src_addr, socklen_t* addrlen)
{
     ssize_t result = 0;
     
     //loop to poll for packet
     while (1)
     {
          result = recvfrom(sockfd, buf, PACKETSIZE, 0, src_addr, addrlen);
          if (result < 0) //either recvfrom has resulted in an error or no data was read
          {
               if (!(errno == EAGAIN || errno == EWOULDBLOCK)) //an error
               {
                    error("Unknown error while attempting to read from socket")
               }
               else if (!block) //no data was read. if this is a non-blocking call, return 0.
                    return 0;
                    
               //continue polling
          }
          else //recvfrom returned a packet
          {
               fprintf(stderr, "lossyrecv received %d bytes\n", result);
               //probability to ignore
               if (rand() <= rdt_lossRate * RAND_MAX)
               {
                    //reset variables and continue the loop if this is a blocking function call.
                    //for non-blocking calls this results in polling for the next packet.
                    //for blocking function calls this results in reading the next packet if it has been received or returning if the next packet has not been received.
                    fprintf(stderr, "lossyrecv ignoring packet\n")
                    result = 0;
                    continue;
               }
               
               //probability to corrupt
               if (rand() <= rdt_corruptRate * RAND_MAX)
               {
                    fprintf(stderr, "lossyrecv corrupting packet\n")
                    return -1;
               }
               
               //packet was received successfully without corruption
               return result;
          }
          
     }
}

