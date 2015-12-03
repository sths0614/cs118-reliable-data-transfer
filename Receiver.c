#include "rdt.c"
#include <sys/socket.h>

int main(int argc, char* argv[]) 
{
	float pLoss, pCorrupt;
	int portNum;
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	socklen_t serv_len;
	char* fname, hostname;
	char* fbuf;

	if (argc < 6){
		error("Wrong number or arguments, need: hostname, portNum, filename, pLoss, pCorrupt\n");
	}
	hostname = argv[1];
	portNum = atoi(argv[2]);
	fname = atoi(argv[3]);
	pLoss = atof(argv[4]);
	pCorrupt = atof(argv[5]);

	if (portNum < 0)	
		error("ERROR: port number must be a positive integer");
	if (pLoss < 0.0 || pLoss > 1.0 || pCorrupt < 0.0 || pCorrupt > 1.0)
		error("ERROR: probabilities need to be between 0.0 and 1.0");

	sockfd = rdt_socket(pCorrupt, pLoss, 1);
	if (sockfd < 0)
		error ("ERROR from rdt_socket()");
	server = gethostbyname(hostname);
	if (server == NULL){
		error ("ERROR no such host\n");
		exit(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; //initialize server's address
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portNum);

    int fsize = rdt_requestReply(sockfd, fname, strlen(fname), &fbuf, 
    	(struct sockaddr*) serv_addr, sizeof(serv_addr));

    // close the socket
	if (rdt_close(sockfd) < 0)
		error ("ERROR from rdt_close()");
	return 0;
}