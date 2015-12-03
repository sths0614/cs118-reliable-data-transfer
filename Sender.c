#include "rdt.c"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char* argv[]) 
{
	float pLoss, pCorrupt;
	int portNum, winSize;
	int sockfd;
	struct sockaddr_in serv_addr;
	char* fname;


	if (argc < 5){
		error("Wrong number or arguments, need: portNum, winSize, pLoss, pCorrupt\n");
	}
	portNum = atoi(argv[1]);
	winSize = atoi(argv[2]);
	pLoss = atof(argv[3]);
	pCorrupt = atof(argv[4]);

	if (portNum < 0)	
		error("ERROR: port number must be a positive integer");
	if (winSize <= 0)	
		error("ERROR: window size must be at least 1");
	if (pLoss < 0.0 || pLoss > 1.0 || pCorrupt < 0.0 || pCorrupt > 1.0)
		error("ERROR: probabilities need to be between 0.0 and 1.0");

	sockfd = rdt_socket(pCorrupt, pLoss, winSize, 100000, 5000000);
	if (sockfd < 0)
		error ("ERROR from rdt_socket()");
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portNum);

	if (rdt_bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
		error ("ERROR from rdt_bind()");

     //only one connection at a time
     while(1)
     {
          socklen_t cli_addrlen = 0;
          rdt_recvfrom(sockfd, &fname, (struct sockaddr*) &cli_addr, &cli_addrlen);
          // rdt_recvfrom(sockfd, &fname, (struct sockaddr*) &cli_addr, 0);

          // open file, get filesize
          FILE* f = fopen(fname, "rb");
          fseek(f, 0L, SEEK_END);
          int fsize = (int) ftell(f);
          fseek(f, 0L, SEEK_SET);
          printf("Filesize is %d\n", fsize);

          // read the file into fbuf
          char* fbuf = (char *)malloc(fsize*sizeof(char));
          fread(fbuf, 1, fsize, f);

          // close file
          fclose(f);

          // Send the file!
          rdt_sendto(sockfd, fbuf, fsize, (struct sockaddr*) &cli_addr, cli_addrlen);

          // free the file buffer
          free(fbuf);
          
          // free file name
          free(fname);
     }

	// close the socket
	if (rdt_close(sockfd) < 0)
		error ("ERROR from rdt_close()");
	return 0;
}