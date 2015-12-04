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
#include <fcntl.h>

#define PACKETSIZE 960
#define PACKET_DAT 0
#define PACKET_END 1
#define PACKET_ACK 2
#define PACKET_DNY 3

static char rdt_recvBuffer[2000];
static char* rdt_sendBuffer;
static ssize_t rdt_sendBufferSize;

static float rdt_corruptRate;
static float rdt_lossRate;
static ssize_t rdt_window;
static long rdt_timeout;
static long rdt_connTimeout;

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
          fprintf(stderr, "address family/length match fail %d and %d vs %d and %d\n",
               addrA->sa_family, *addrlenA,
               addrB->sa_family, *addrlenB
          );
          return -1;
     }
     int i;
     for (i = 0; i < 8; i++)
          if (addrA->sa_data[i] != addrB->sa_data[i])
          {
               fprintf(stderr, "address value match fail %d vs %d at %d\n",
                    addrA->sa_data[i],
                    addrB->sa_data[i],
                    i
               );
               return -1;
          }
     return 0;
}

int rdt_socket(float corruptRate, float lossRate, ssize_t window, long timeout, long connTimeout)
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
     if (connTimeout > 0)
          rdt_connTimeout = connTimeout;
     int sock = socket(AF_INET, SOCK_DGRAM, 0);
     //fcntl(sock, F_SETFL, O_NONBLOCK);
     return sock;
}

int rdt_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
     return bind(sockfd, addr, addrlen);
}

int rdt_close(int sockfd)
{
     return close(sockfd);
}

ssize_t rdt_sendto(int sockfd, const void *buf, size_t len, const struct sockaddr *dest_addr, socklen_t addrlen)
{
     fprintf(stderr, "sendto sending to %d %d %d %d %d %d %d\n",
          (dest_addr->sa_data[0] & 0xFF) * 256 +
          (dest_addr->sa_data[1] & 0xFF),
          dest_addr->sa_data[2] & 0xFF,
          dest_addr->sa_data[3] & 0xFF,
          dest_addr->sa_data[4] & 0xFF,
          dest_addr->sa_data[5] & 0xFF,
          dest_addr->sa_data[6] & 0xFF,
          dest_addr->sa_data[7] & 0xFF
     );
     long packetTot;
     int packetRemainder;
     
     if (len == 0)
     {
          packetTot = 0;
          packetRemainder = 0;
     }
     else if (len == (size_t)(-1))
     {
          len = 0;
          packetTot = -1;
          packetRemainder = 0;
     }
     else
     {
          packetTot = (len-1) / (PACKETSIZE-2); //total number of data packets to be sent, minus 1
          packetRemainder = ((len-1) % (PACKETSIZE-2) + 1);  //amount of data to be transmitted in the last
     }
     
     fprintf(stderr, "sendto got buffer length %d resulting in %ld plus 1 packets with last packet having size %d\n", len, packetTot, packetRemainder);
     long packetNum = 0;
     int seqNumBack = 0;
     int seqOffset = 0;
     struct sockaddr recvaddr;
     socklen_t recvaddrlen;
     char* bufC = (char*)buf;
     char pktBuf[PACKETSIZE];
     struct timeval timeStamp;
     struct timeval timeStampConn;
     struct timeval timeNow;
     ssize_t result;     
     ssize_t sresult;
     
     gettimeofday(&timeStamp,NULL);
     gettimeofday(&timeStampConn,NULL);
     while (1)
     {
          //receive packet
          result = lossyrecv(sockfd, (void*)pktBuf, 0, &recvaddr, &recvaddrlen);
          if (result >= 0)
          {
               fprintf(stderr, "sendto got packet from %d %d %d %d %d %d %d; type is %d; addrlen is %d\n",
                    (recvaddr.sa_data[0] & 0xFF) * 256 +
                    (recvaddr.sa_data[1] & 0xFF),
                    recvaddr.sa_data[2] & 0xFF,
                    recvaddr.sa_data[3] & 0xFF,
                    recvaddr.sa_data[4] & 0xFF,
                    recvaddr.sa_data[5] & 0xFF,
                    recvaddr.sa_data[6] & 0xFF,
                    recvaddr.sa_data[7] & 0xFF,
                    recvaddr.sa_family,
                    recvaddrlen
               );
               
               fprintf(stderr, "sendto compare to      %d %d %d %d %d %d %d; type is %d; addrlen is %d\n",
                    (dest_addr->sa_data[0] & 0xFF) * 256 +
                    (dest_addr->sa_data[1] & 0xFF),
                    dest_addr->sa_data[2] & 0xFF,
                    dest_addr->sa_data[3] & 0xFF,
                    dest_addr->sa_data[4] & 0xFF,
                    dest_addr->sa_data[5] & 0xFF,
                    dest_addr->sa_data[6] & 0xFF,
                    dest_addr->sa_data[7] & 0xFF,
                    dest_addr->sa_family,
                    addrlen
               );
          }
          if (result >= 0 && rdt_checkAddrMatch(&recvaddr, dest_addr, &recvaddrlen, &addrlen))
          {
               fprintf(stderr, "sendto got address mismatch\n");
               pktBuf[0] = PACKET_DNY;
               sresult = sendto(sockfd, pktBuf, 1, 0, &recvaddr, recvaddrlen);
               if (sresult != 1)
                    fprintf(stderr,"sendto encountered an error while sending DNY packet\n");
          }
          else if (result == 2 && pktBuf[0] == PACKET_ACK) //received ACK, update window
          {
               gettimeofday(&timeStampConn,NULL); //reset timer
               int reqNum = pktBuf[1];
               fprintf(stderr, "sendto got ack # %d; current packet # is %ld; current base is %d\n", reqNum, packetNum, seqNumBack);
               if (mod256LessThan(seqNumBack, reqNum))
               {
                    gettimeofday(&timeStamp,NULL); //reset timer
                    int advancement = mod256Sub(reqNum,seqNumBack);
                    packetNum += advancement; //increment the number for which packet we are trying to send
                    seqNumBack = reqNum;
                    seqOffset -= advancement;
               }
          }
          else if (result == 2 && pktBuf[0] == PACKET_END) //this situation happens when server has switched to send mode but client has not
          {
               gettimeofday(&timeStampConn,NULL); //reset timer
               fprintf(stderr, "sendto got END packet!\n");
               int reqNum = pktBuf[1];
               pktBuf[0] = PACKET_ACK;
               pktBuf[1] = (reqNum +1) & 0xFF;
          }
          
          gettimeofday(&timeNow,NULL); //get time
          //if timeout then mark all packets in the window as unsent
          if (timeDiff(&timeNow,&timeStamp) >= rdt_timeout)
          {
               fprintf(stderr, "sendto timeout! seqOffset was %d\n", seqOffset);
               gettimeofday(&timeStamp,NULL); //reset timer
               seqOffset = 0;
          }
          
          //waited too long for response; connection is considered closed.
          if (timeDiff(&timeNow,&timeStampConn) >= rdt_connTimeout)
          {
               fprintf(stderr, "sendto connection timeout! at packet # %ld of %ld\n", packetNum, packetTot);
               return -1;
          }
          
          if (packetNum <= packetTot) //not all packets of the message have been sent
          {
               
               //send all unsent packets in the window
               for (; seqOffset < rdt_window && (packetNum + seqOffset) <= packetTot; seqOffset++)
               {
                    pktBuf[0] = PACKET_DAT;
                    pktBuf[1] = (seqNumBack + seqOffset) & 0xFF;
                    
                    fprintf(stderr, "sendto sending packet # %ld with seq # %d at bufpos %ld\n", packetNum + seqOffset, pktBuf[1], (packetNum + seqOffset) * (PACKETSIZE-2));
                    if (packetNum + seqOffset == packetTot) //we are on the last packet
                    {
                         memcpy(pktBuf + 2, buf + (packetNum + seqOffset) * (PACKETSIZE-2), packetRemainder);
                         fprintf(stderr,"send\n");
                         sresult = sendto(sockfd, pktBuf, packetRemainder+2, 0, dest_addr, addrlen);
                         if (sresult != packetRemainder+2)
                              error("sendto encountered an error while sending DAT packet");
                         /*
                         sresult = -1;
                         while (sresult != packetRemainder)
                         {
                              sresult = sendto(sockfd, pktBuf, packetRemainder, 0, dest_addr, addrlen);
                              if (sresult != packetRemainder && errno != EAGAIN)
                                   error("sendto encountered an error while sending DAT packet");
                              fprintf(stderr, ".");
                         }
                         fprintf(stderr, "\n");
                         */
                    }
                    else
                    {
                         memcpy(pktBuf + 2, buf + (packetNum + seqOffset) * (PACKETSIZE-2), PACKETSIZE-2);
                         fprintf(stderr,"send\n");
                         sresult = sendto(sockfd, pktBuf, PACKETSIZE, 0, dest_addr, addrlen);
                         if (sresult != PACKETSIZE)
                              error("sendto encountered an error while sending DAT packet");
                         /*
                         sresult = -1;
                         while (sresult != PACKETSIZE-2)
                         {
                              sresult = sendto(sockfd, pktBuf, PACKETSIZE-2, 0, dest_addr, addrlen);
                              if (sresult != PACKETSIZE-2 && errno != EAGAIN)
                                   error("sendto encountered an error while sending DAT packet");
                              fprintf(stderr, ".");
                         }
                         fprintf(stderr, "\n");
                         */
                    }
               }   
          }
          else if (packetNum == packetTot + 1) //all packets have been sent and ACKed, now sending confirmation of END
          {
               if (seqOffset != rdt_window)
               {
                    fprintf(stderr, "sendto sending END packet\n");
                    pktBuf[0] = PACKET_END;
                    pktBuf[1] = seqNumBack;
                    sresult = sendto(sockfd, pktBuf, 2, 0, dest_addr, addrlen);
                    if (sresult != 2)
                         error("sendto encountered an error while sending END packet");
                    /*
                    sresult = -1;
                    while (sresult != 2)
                    {
                         sresult = sendto(sockfd, pktBuf, 2, 0, dest_addr, addrlen);
                         if (sresult != PACKETSIZE-2 && errno != EAGAIN)
                              error("sendto encountered an error while sending END packet");
                         fprintf(stderr, ".");
                    }
                    fprintf(stderr, "\n");
                    */
               }
               seqOffset = rdt_window;
          }
          else //ACK has been received for END, exit the function
               break;
     }
     return len;
}

ssize_t lossyrecv(int sockfd, void *buf, int block, struct sockaddr *src_addr, socklen_t* addrlen)
{
     ssize_t result = 0;
     
     //loop to poll for packet
     while (1)
     {
          result = recvfrom(sockfd, buf, PACKETSIZE, MSG_DONTWAIT, src_addr, addrlen);
          /*
          fprintf(stderr, "lossyrecv got packet from %d %d %d %d %d %d %d\n",
               (src_addr->sa_data[0] & 0xFF) * 256 +
               (src_addr->sa_data[1] & 0xFF),
               src_addr->sa_data[2] & 0xFF,
               src_addr->sa_data[3] & 0xFF,
               src_addr->sa_data[4] & 0xFF,
               src_addr->sa_data[5] & 0xFF,
               src_addr->sa_data[6] & 0xFF,
               src_addr->sa_data[7] & 0xFF
          );*/
          if (result < 0) //either recvfrom has resulted in an error or no data was read
          {
               if (!(errno == EAGAIN || errno == EWOULDBLOCK)) //an error
               {
                    error("Unknown error while attempting to read from socket");
               }
               else if (!block) //no data was read. if this is a non-blocking call, return 0.
                    return -1;
                    
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
                    fprintf(stderr, "lossyrecv ignoring packet\n");
                    result = 0;
                    continue;
               }
               
               //probability to corrupt
               if (rand() <= rdt_corruptRate * RAND_MAX)
               {
                    fprintf(stderr, "lossyrecv corrupting packet\n");
                    return -1;
               }
               
               //packet was received successfully without corruption
               return result;
          }
          
     }
}

ssize_t rdt_recvfrom(int sockfd, void **buf, struct sockaddr *src_addr, socklen_t *addrlen)
{
     
     ssize_t result = 0;
     ssize_t sresult;
     ssize_t allocLength = 4*PACKETSIZE;
     ssize_t length = 0;
     //ssize_t msgLength = 0;
     struct sockaddr cmpaddr;
     socklen_t cmpaddrlen = 0;
     struct sockaddr recvaddr;
     socklen_t recvaddrlen;
     //int lengthObtained = 0;
     int reqNum = 0;
     int invalid_addr = 0;
     char reply[2];
     char* msgBuf = malloc(4*PACKETSIZE);
     char pktBuf[PACKETSIZE];
     int datReceived = 0;
     
     if (*addrlen != 0)
     {
          cmpaddr.sa_family = src_addr->sa_family;
          memcpy(cmpaddr.sa_data, src_addr->sa_data, *addrlen - sizeof(sa_family_t));
          cmpaddrlen = *addrlen;
     }
     else
     {
          fprintf(stderr, "recvfiltered listening to any source\n");
     }
     
     
     fprintf(stderr, "recvfiltered wait\n");
     while (1)
     {
          //wait for packet receive
          invalid_addr = 0;
          result = lossyrecv(sockfd, (void*)pktBuf, 1, &recvaddr, &recvaddrlen);
          fprintf(stderr, "recvfiltered got packet from %d %d %d %d %d %d %d; type is %d; addrlen is %d\n",
               (recvaddr.sa_data[0] & 0xFF) * 256 +
               (recvaddr.sa_data[1] & 0xFF),
               recvaddr.sa_data[2] & 0xFF,
               recvaddr.sa_data[3] & 0xFF,
               recvaddr.sa_data[4] & 0xFF,
               recvaddr.sa_data[5] & 0xFF,
               recvaddr.sa_data[6] & 0xFF,
               recvaddr.sa_data[7] & 0xFF,
               recvaddr.sa_family,
               recvaddrlen
          );
          
          if (result <= 0)
          {
               fprintf(stderr, "recvfiltered got packet with size 0\n");
               continue;
          }
          
          int packetType = pktBuf[0];
          
          fprintf(stderr, "recvfiltered compare to      %d %d %d %d %d %d %d; type is %d; addrlen is %d\n",
               (cmpaddr.sa_data[0] & 0xFF) * 256 +
               (cmpaddr.sa_data[1] & 0xFF),
               cmpaddr.sa_data[2] & 0xFF,
               cmpaddr.sa_data[3] & 0xFF,
               cmpaddr.sa_data[4] & 0xFF,
               cmpaddr.sa_data[5] & 0xFF,
               cmpaddr.sa_data[6] & 0xFF,
               cmpaddr.sa_data[7] & 0xFF,
               cmpaddr.sa_family,
               cmpaddrlen
          );
          
          if (packetType == PACKET_DNY)
          {
               fprintf(stderr, "recvfiltered got DNY\n");
          }
          
          //check that address is what we want, or if no desired address is specified, then accept the packet
          if (cmpaddrlen != 0)
          {
               if (rdt_checkAddrMatch(&recvaddr, &cmpaddr, &recvaddrlen, &cmpaddrlen))
               {
                    fprintf(stderr, "recvfiltered got address mismatch\n");
                    reply[0] = PACKET_DNY;
                    sresult = sendto(sockfd, reply, 1, 0, &recvaddr, recvaddrlen);
                    if (sresult != 1)
                         fprintf(stderr,"recvfiltered encountered an error while sending DNY packet\n");
                    continue;
               }
          }
          else if (packetType == PACKET_DAT)
          {
               src_addr->sa_family = cmpaddr.sa_family = recvaddr.sa_family;
               memcpy(cmpaddr.sa_data, recvaddr.sa_data, recvaddrlen - sizeof(sa_family_t));
               memcpy(src_addr->sa_data, recvaddr.sa_data, recvaddrlen - sizeof(sa_family_t));
               *addrlen = cmpaddrlen = recvaddrlen;
          }
          
          if (packetType == PACKET_DAT)
          {
               datReceived = 1;
          }
          
          int seqNum = pktBuf[1];
          
          //get sequence number
          int copy = 0;
          if ((reqNum & 0xFF) == (seqNum & 0xFF))
          {
               copy = 1;
               fprintf(stderr, "recvfiltered got packet with seqNum %d (correct)\n", seqNum);
               reqNum = (reqNum + 1) & 0xFF;
          }
          else
          {
               fprintf(stderr, "recvfiltered got packet with seqNum %d (incorrect, wanted %d)\n", seqNum, reqNum);
          }
          
          //reply with ack
          reply[0] = PACKET_ACK;
          reply[1] = reqNum;
          fprintf(stderr, "recvfiltered sending ACK %d\n", reqNum);
          sresult = sendto(sockfd, reply, 2, 0, &recvaddr, recvaddrlen);
          if (sresult != 2)
               error("recvfiltered encountered an error while sending ACK packet");
          /*
          sresult = -1;
          while (sresult != 2)
          {
               sresult = sendto(sockfd, reply, 2, 0, &recvaddr, recvaddrlen);
               if (sresult != 2 && errno != EAGAIN)
                    error("recvfiltered encountered an error while sending ACK packet");
               fprintf(stderr, ".");
          }
          fprintf(stderr, "\n");
          */
          
          //packet type
          if (packetType == PACKET_END)
          {
               fprintf(stderr, "recvfiltered got END packet\n");
               break;
          }
          
          //if there is not enough space on the received message buffer, expand it
          if (allocLength - length < PACKETSIZE)
          {
               allocLength += allocLength/2;
               msgBuf = realloc(msgBuf, allocLength);
               if (msgBuf == NULL)
                    error("recvfiltered ran out of memory while expanding the message buffer!");
          }
          
          //copy packet contents to buffer
          if (copy)
          {
               memcpy((void*)msgBuf + length, (void*)pktBuf + 2, result - 2);
               length += (result - 2);
               msgBuf[length] = 0;
          }
     }
     
     if (datReceived)
          *buf = (void*)msgBuf;
     else
          *buf = NULL;
     fprintf(stderr, "recvfiltered exit\n");
     return length;
}

ssize_t rdt_requestReply(int sockfd, const void* buf, size_t len, void** recvbuf, struct sockaddr* dest_addr, socklen_t addrlen)
{
     ssize_t result;
     ssize_t rlen;
     result = rdt_sendto(sockfd, buf, len, dest_addr, addrlen);
     rlen = rdt_recvfrom(sockfd, recvbuf, dest_addr, &addrlen);
     return rlen;
}
