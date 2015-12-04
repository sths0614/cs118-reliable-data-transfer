#include "rdt.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char* argv[]) 
{
     float pLoss, pCorrupt;
     int portNum, winSize;
     int cliPort;
     int sockfd;
     struct sockaddr_in serv_addr;
     struct sockaddr_in cli_addr;
     struct hostent *server;
     socklen_t serv_len;
     char* fname;
     char* hostname;
     char* fbuf;

     if (argc < 8){
          error("Wrong number or arguments, need: hostname, srvPort, cliPort, winSize, filename, pLoss, pCorrupt\n");
     }
     hostname = argv[1];
     portNum = atoi(argv[2]);
     cliPort = atoi(argv[3]);
     winSize = atoi(argv[4]);
     fname = argv[5];
     pLoss = atof(argv[6]);
     pCorrupt = atof(argv[7]);

     if (portNum < 0)	
     error("ERROR: port number must be a positive integer");
     if (pLoss < 0.0 || pLoss > 1.0 || pCorrupt < 0.0 || pCorrupt > 1.0)
     error("ERROR: probabilities need to be between 0.0 and 1.0");

	sockfd = rdt_socket(pCorrupt, pLoss, winSize, 10000, 5000000);
     if (sockfd < 0)
     error ("ERROR from rdt_socket()");

	bzero((char *) &cli_addr, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = INADDR_ANY;
	cli_addr.sin_port = htons(cliPort);

	if (rdt_bind(sockfd, (struct sockaddr*) &cli_addr, sizeof(cli_addr)) < 0)
		error ("ERROR from rdt_bind()");


     server = gethostbyname(hostname);
     if (server == NULL){
          error ("ERROR no such host\n");
          exit(1);
     }

     bzero((char *) &serv_addr, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET; //initialize server's address
     bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
     serv_addr.sin_port = htons(portNum);

     long fsize = rdt_requestReply(sockfd, fname, strlen(fname), (void**)&fbuf, 
     (struct sockaddr*) &serv_addr, sizeof(struct sockaddr_in));

     // close the socket
     if (rdt_close(sockfd) < 0)
     error ("ERROR from rdt_close()");

     if (fbuf)
     {
          fprintf(stderr, "Writing file\n");
          // write buffer to file
          FILE* f = fopen(fname, "wb");
          fwrite(fbuf, 1, fsize, f);
          fclose(f);
     }
     else
     {
          fprintf(stderr, "File not found on server\n");
     }
     
     // free buffer
     free(fbuf);
     
     return 0;
}