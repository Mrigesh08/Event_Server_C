#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFSIZE 10000

int main(){

	int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0){
		printf ("Error in opening a socket"); exit (0);
	}
	printf ("Client Socket Created\n");
	
	/*CONSTRUCT SERVER ADDRESS STRUCTURE*/
	struct sockaddr_in serverAddr;
	memset (&serverAddr,0,sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(12345);
	serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	printf ("Address assigned\n");
	
	/*ESTABLISH CONNECTION*/
	int c = connect (sock, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
	printf ("%d\n",c);
	if (c < 0){
		printf ("Error while establishing connection");
		exit (0);
	}
	printf("Connection Established\n");
	char buffer[BUFSIZE];
	int k=recv(sock,buffer,BUFSIZE,0);
	if(k!=0){
		printf("Printing buffer contents:\n");
		printf("%s\n",buffer );
	}
	
	close(sock);
}