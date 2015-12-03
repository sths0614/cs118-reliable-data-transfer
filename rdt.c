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
#include <time.h>

static char rdt_recvBuffer[2000];
static char* rdt_sendBuffer;
static ssize_t rdt_sendBufferSize;

static float rdt_corruptRate;
static float rdt_lossRate;
static ssize_t rdt_window;
static long rdt_timeout;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

long timeDiff(struct timeval *tA, struct timeval *tB)
{
    return (tA->tv_usec + 1000000 * tA->tv_sec) - (tB->tv_usec + 1000000 * tB->tv_sec);
}

int mod256LessThan(int A, int B)
{
     if ((B - A) < 128 && (B - A) > 0)
          return 1;
     else if ((A - B) > 128)
     {
          return 1;
     }
     return 0;
}

int mod256Sub(int A, int B)
{
     return ((A & 0xFF) - (B & 0xFF)) & 0xFF;
}

int rdt_checkAddrMatch(const struct sockaddr* addrA, const struct sockaddr* addrB, socklen_t* addrlenA, socklen_t* addrlenB)
{
     if (!(addrA->sa_family == addrB->sa_family && *addrlenA == *addrlenB))
     {
          fprintf(stderr, "address family/length match fail\n");
          return -1;
     }
     for (int i = 0; i < *addrlenA-sizeof(sa_family_t); i++)
          if (addrA->sa_data[i] != addrB->sa_data[i])
          {
               fprintf(stderr, "address value match fail\n");
               return -1;
          }
     return 0;
}

int rdt_socket(float corruptRate, float lossRate, ssize_t window, long timeout)
{
     time_t t;
     srand((unsigned) time(&t));
     
     if (corruptRate >= 0)
          rdt_corruptRate = corruptRate;
     if (corruptRate >= 0)
          rdt_lossRate = lossRate;
     if (window > 0 && window < 128)
          rdt_window = window;
     if (timeout > 0)
          rdt_timeout = timeout;
     int sock = socket(AF_INET, SOCK_DGRAM, 0);
     fcntl(sock, F_SETFL, O_NONBLOCK);
     return sock;
}

int rdt_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
     return bind(sockfd, addr, addrlen);
}

int rdt_close(int sockfd);
{
     return close(sockfd);
}

ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
     if (len == 0)
          return 0;
     long packetTot = (len-1) / (PACKETSIZE-2); //total number of data packets to be sent, minus 1
     int packetRemainder = ((len-1) % (PACKETSIZE-2) + 1);  //amount of data to be transmitted in the last packet
     long packetNum = 0;
     int seqNumBack = 0;
     int seqOffset = 0;
     struct sockaddr recvaddr;
     socklen_t recvaddrlen;
     char* bufC = (char*)buf;
     char* pktBuf[PACKETSIZE];
     struct timeval timeStamp;
     struct timeval timeNow;
     
     gettimeofday(&timeStamp,NULL);
     while (1)
     {
          //receive packet
          result = lossyrecv(sockfd, (void*)pktBuf, 0, &recvaddr, &recvaddrlen);
          if (checkAddrMatch(&recvaddr, dest_addr, &recvaddrlen, addrlen))
          {
               fprintf(stderr, "sendto got address mismatch\n");
               pktBuf[0] = PACKET_DNY;
               sendto(sockfd, pktBuf, 1, 0, &recvaddr, recvaddrlen);
          }
          else if (result > 0 && pktBuf[0] == PACKET_ACK) //received ACK, update window
          {
               int reqNum = pktBuf[1];
               fprintf(stderr, "sendto got ack # %d\n", reqNum);
               if (mod256LessThan(seqNumBack, reqNum))
               {
                    gettimeofday(&timeStamp,NULL); //reset timer
                    int advancement = mod256sub(reqNum,seqNumBack);
                    packetNum += advancement; //increment the number for which packet we are trying to send
                    seqNumBack = reqNum;
                    seqOffset -= advancement;
               }
          }
          
          
          gettimeofday(&timeNow,NULL); //get time
          //if timeout then mark all packets in the window as unsent
          if (timeDiff(&timeNow,&timeStamp) >= rdt_timeout)
          {
               seqOffset = 0;
          }
          
          if (packetNum <= packetTot) //not all packets of the message have been sent
          {
               
               //send all unsent packets in the window
               for (; seqOffset < rdt_window && (packetNum + seqOffset) <= packetTot; seqOffset++)
               {
                    pktBuf[0] = PACKET_DAT;
                    pktBuf[1] = (seqNumBack + seqOffset) & 0xFF;
                    if (packetNum == packetTot) //we are on the last packet
                    {
                         memcpy(pktBuf + 2, bufC + (packetNum + seqOffset) * (PACKETSIZE-2), packetRemainder);
                         sendto(sockfd, pktBuf, packetRemainder, 0, dest_addr, addrlen);
                    }
                    else
                    {
                         memcpy(pktBuf + 2, bufC + (packetNum + seqOffset) * (PACKETSIZE-2), PACKETSIZE-2);
                         sendto(sockfd, pktBuf, PACKETSIZE-2, 0, dest_addr, addrlen);
                    }
               }   
          }
          else if (packetNum == packetTot + 1) //all packets have been sent and ACKed, now sending confirmation of END
          {
               if (seqOffset != rdt_window)
               {
                    pktBuf[0] = PACKET_END;
                    sendto(sockfd, pktBuf, 1, 0, dest_addr, addrlen);
               }
               seqOffet = rdt_window;
          }
          else //ACK has been received for END, exit the function
               break;
     }
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
                    //reset variables and continue the loop.
                    //for blocking calls this results in polling for the next packet.
                    //for non-blocking function calls this results in reading the next packet if it has been received or returning if the next packet has not been received.
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

ssize_t rdt_recvfiltered(int sockfd, void *buf, const struct sockaddr *src_addr, const socklen_t addrlen)
{
     ssize_t result = 0;
     ssize_t allocLength = 4*PACKETSIZE;
     ssize_t length = 0;
     //ssize_t msgLength = 0;
     struct sockaddr cmpaddr;
     socklen_t cmpaddrlen;
     struct sockaddr recvaddr;
     socklen_t recvaddrlen;
     //int lengthObtained = 0;
     int reqNum = 0;
     int invalid_addr = 0;
     char reply[2];
     char* msgBuf = malloc(4*PACKETSIZE);
     char* pktBuf[PACKETSIZE];
     
     if (src_addr != NULL && addrlen != 0)
     {
          cmpaddr.sa_family = src_addr->sa_family;
          cmpaddr.sa_data = src_addr->sa_data;
          cmpaddrlen = addrlen;
     }
     
     while (1)
     {
          //wait for packet receive
          invalid_addr = 0;
          result = lossyrecv(sockfd, (void*)pktBuf, 1, &recvaddr, &recvaddrlen);
          
          if (result <= 0)
          {
               fprintf(stderr, "recvfiltered got packet with size 0\n");
               continue;
          }
          
          int packetType = pktBuf[0];
          
          //check that address is what we want, or if no desired address is specified, then accept the packet
          if (cmpaddrlen != 0)
          {
               if (checkAddrMatch(&recvaddr, src_addr, &recvaddrlen, addrlen))
               {
                    fprintf(stderr, "recvfiltered got address mismatch\n");
                    reply[0] = PACKET_DNY;
                    reply[1] = 0;
                    sendto(sockfd, reply, 2, 0, &recvaddr, recvaddrlen);
                    continue;
               }
          }
          else if (packetType == PACKET_DAT)
          {
               cmpaddr.sa_family = recvaddr.sa_family;
               cmpaddr.sa_data = recvaddr.sa_data;
               cmpaddrlen = addrlen;
          }
          
          //packet type
          if (packetType == PACKET_END)
               break;
          
          int seqNum = pktBuf[1];
          
          //get sequence number
          if (reqNum == seqNum)
          {
               fprintf(stderr, "recvfiltered got packet with seqNum %d (correct)\n", seqNum);
               reqNum = (reqNum + 1) & 0xFF;
          }
          else
          {
               fprintf(stderr, "recvfiltered got packet with seqNum %d (incorrect)\n", seqNum);
          }
          
          //reply with ack
          reply[0] = PACKET_ACK;
          reply[1] = reqNum;
          sendto(sockfd, reply, 2, 0, &recvaddr, recvaddrlen);
          
          //if there is not enough space on the received message buffer, expand it
          if (allocLength - length < PACKETSIZE)
          {
               allocLength += allocLength/2;
               msgBuf = realloc(msgBuf, allocLength);
               if (msgBuf == NULL)
                    error("recvfiltered ran out of memory while expanding the message buffer!");
          }
          
          //copy packet contents to buffer
          memcpy((void*)msgBuf + length, (void*)pktBuf + 2, result - 2);
          length += (result - 2);
     }
     
     return length;
}
