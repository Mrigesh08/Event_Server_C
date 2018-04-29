#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <aio.h>
#include <signal.h>

#define MAXPENDING 10
#define buf_size 100 // to read data from file using aio
#define MAX_EVENTS 1000
#define IO_SIGNAL SIGUSR1 

#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

int gotSIGQUIT;

struct ioRequest{
	int lastRequestFlag; // 0 if it is an intermediate request and 1 if is the last request
	char * buf;
	int sockfd; // stores the client socket connfd
	struct aiocb * aiocbList; // used so that the structure can be freed to save up memory
	struct aiocb * aioInstance; // used to close the corresponding file descriptor
	struct ioRequest * start; // used to free up space
};
/* Handler for SIGQUIT */
static void quitHandler(int sig){
    gotSIGQUIT = 1;
}

/* Handler for I/O completion signal */
static void aioSigHandler(int sig, siginfo_t *si, void *ucontext){
	if(sig == SIGRTMIN +1){
		struct ioRequest * io = si->si_value.sival_ptr;

	    // send the data
	    // free the buffer;
	    if(io->lastRequestFlag == 1){
		    char str[strlen(io->buf)+100];
		    // memset(str,'\0',sizeof(str)); // not listed as a safe function to call inside a signal handler
		    sprintf(str,"HTTP/1.1 200 OK\r\n" // not listed as a safe function to call inside a signal handler
		    			"Content-Length: %d\r\n"
		    			"Content-Type: text/html\r\n"
		    			"\r\n"
		    			"%s\r\n"
		    			"\r\n",(int)strlen(io->buf),io->buf);

		   	// printf("%s\n",str );
		   	int k=send(io->sockfd,str,strlen(str)+1,0);
		    if(k!=strlen(str)+1){
		    	// printf("problem in sending. %d bytes sent.\n", k);
		    	// perror("send2");
		    }
		    else{
		    	// printf("data sent\n");
		    }
		}
		
		close(io->aioInstance->aio_fildes);
		// sending a large amount of data through send call was taking time
	    // by that time program control came to close stmt and closed the connection
	    // thereby causing the application to not send the whole data.
	    // Therefore, keep the close call commented for large data (>= 1 MB)
	    close(io->sockfd); 
	}
	
}

int main (){

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = quitHandler;
    if (sigaction(SIGQUIT, &sa, NULL) == -1)
        errExit("sigaction");

    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sa.sa_sigaction = aioSigHandler;
    if (sigaction(SIGRTMIN +1, &sa, NULL) == -1)
        errExit("sigaction");

	/*CREATE A TCP SOCKET*/
	int server_socket = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(server_socket < 0){
		printf ("Error while server socketcreation");
		exit (0);
	}
	printf ("Server Socket Created\n");

	/*CONSTRUCT LOCAL ADDRESS STRUCTURE*/
	struct sockaddr_in server_address, client_address;
	memset (&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(12345);
	server_address.sin_addr.s_addr = htonl(INADDR_ANY);
	printf("Server address assigned\n");
	// printf("ip=%s,%d\n",inet_ntoa(server_address.sin_addr), htons(server_address.sin_port));

	int temp = bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address));
	if(temp < 0){
		printf ("Error while binding\n");
		exit (0);
	}
	printf ("Binding successful\n");

	int temp1=listen(server_socket, MAXPENDING);
	if (temp1 < 0){
		printf ("Error in listen");
		exit (0);
	}
	printf ("Now Listening\n");

	int client_length = sizeof(client_address);

	int current_socket, bytes_sent, bytes_received;

	// ========================================
	// EPOLL starts here
	// ========================================
	int epfd=epoll_create(12000);
	if(epfd == -1){
		perror("epoll_create");
		exit(0);
	}

	struct epoll_event ev;
	struct epoll_event * evlist=(struct epoll_event *)malloc(sizeof(struct epoll_event )*MAX_EVENTS);
	ev.events = EPOLLIN;
	ev.data.fd=server_socket;
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, server_socket, &ev)==-1){
		perror("epoll_ctl");
		exit(0);
	}
	while(1){
		int x=epoll_wait(epfd,evlist, MAX_EVENTS,-1);
		if(x==-1){
			if(errno== EINTR)
				continue;
			else{
				perror("epoll_wait");
				exit(0);
			}
		}
		else{
			// printf("NO ERROR\n");
		}
		
		for(int i=0;i<x;i++){
			// printf("INSIDE FOR i=%d\n",i);
			if(evlist[i].events & (EPOLLIN | EPOLLOUT)){
				if(evlist[i].data.fd==server_socket){
					// printf("INSIDE IF\n");
					// add the new user
					int connfd=accept(server_socket, (struct sockaddr *)&client_address,&client_length);
					if(connfd==-1){
						perror("accept");
						exit(0);
					}
					struct epoll_event ev2;
					ev2.events = EPOLLIN | EPOLLOUT; // looking for readable/ writable events
					ev2.data.fd = connfd;
					if(epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev2)==-1){
						perror("epoll_ctl2");
						exit(0);
					}
					printf("New Connection Established with connfd as %d\n",connfd );

				}
				else{	
					// assuming that it is a GET reqest from httperf or anywhere else :P
					// printf("INSIDE ELSE\n");
					int bytesRead=0;
					FILE * fp=fopen("sampleFile","r");
					if(fp==NULL){
						perror("fopen");
						printf("A file is required from which data can be read and sent to the client.\n");
						exit(0);
					}	
					fseek(fp,0, SEEK_END);
					int fSize=ftell(fp);
					fseek(fp,0,SEEK_SET);
					fclose(fp);
					char * buffer=(char *)malloc(sizeof(char)*(fSize+10));
					memset(buffer,'\0',fSize+10);
					// for each request, an aiocb structure is needed
					
					int numReqs;
					printf("FILE SIZE %d\n",fSize);
					if(fSize % buf_size == 0)
						numReqs = fSize / buf_size;
					else
						numReqs = fSize / buf_size + 1;

					struct aiocb * aiocbList = (struct aiocb *)malloc(sizeof(struct aiocb)*numReqs);
					struct ioRequest * ioList = (struct ioRequest *)malloc(sizeof(struct ioRequest)*numReqs);
					
					int j=0;
					int filefd=open("sampleFile",O_RDONLY);
					while(numReqs > 0){
						// printf("HERE F\n");
						memset(&ioList[j],0,sizeof(struct ioRequest));
						memset(&aiocbList[j],0,sizeof(struct aiocb));
						// printf("1\n");
						if(numReqs==1)
							ioList[j].lastRequestFlag=1;
						else
							ioList[j].lastRequestFlag=0;
						ioList[j].buf = buffer;
						ioList[j].sockfd = evlist[i].data.fd;
						ioList[j].aiocbList = aiocbList;
						ioList[j].aioInstance = &aiocbList[j];
						ioList[j].start = ioList;
						aiocbList[j].aio_fildes = filefd;
						aiocbList[j].aio_offset = bytesRead;
						aiocbList[j].aio_buf = buffer + bytesRead;
						aiocbList[j].aio_nbytes = buf_size;
						aiocbList[j].aio_reqprio = 0;
						// only send a signal when the buffer will be completely full
						if(numReqs==1)
							aiocbList[j].aio_sigevent.sigev_notify = SIGEV_SIGNAL;
						else
							aiocbList[j].aio_sigevent.sigev_notify = SIGEV_NONE;
						aiocbList[j].aio_sigevent.sigev_signo = SIGRTMIN +1;
						aiocbList[j].aio_sigevent.sigev_value.sival_ptr = &ioList[j];
						// printf("2\n");
						int r=aio_read(&aiocbList[j]);
						if(r==-1)
							errExit("aio_read");
						else{
							// printf("%d bytes queued\n", buf_size);
							bytesRead+=buf_size;
						}
						numReqs--;
						j++;
					}
					// delete this fd from the epoll create instance because
					// due to aio, the send request completes at a later time
					// during which the for loop keeps going on and finds that the client socket is avalable for writing 
					// and tries to initiate more writes.
					if(epoll_ctl(epfd, EPOLL_CTL_DEL, evlist[i].data.fd, NULL)==-1){
						perror("epoll_ctl3");
						// exit(0);
					}
					// for multiple get requests, 
					// the evlist[i].data.fd can be added to epoll after the io is over completely.
				}
			}
		}
	}
	close (server_socket);
}