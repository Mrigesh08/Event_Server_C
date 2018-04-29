# Event Driven Server

We have impleted the event driven server using epoll and asynchronous IO. For asynchronous IO, we have used the “aio.h” file. 
The program creates and binds address to a socket instance. This socket instance is added to the watchlist of an epoll instance (looking for only EPOLLIN events). When a new connection comes up, that is also added to the watchlist of epoll instance (looking for both EPOLLIN and EPOLLOUT events).
After the connection is established, we wait for a GET request from the client. When that request comes, we queue multiple asynchronous read requests to read from a file into a buffer. Realtime signals are used to notify that aysnchronous read is complete. After that, the data is wrapped in an http reply and sent back to the client. 
The psuedocode for the whole program is as follows-
	
	main(){
		setSignalHandler(SIGRTMIN+1,aioSigHandler)
		server_soket=socket();
		bind();
		listen();
		epfd=epoll_create();
		epoll_ctl(server_socket); // add the server_socket
		while(1){
			x=epoll_wait(epfd);
			for (i =0 to x){
				if(evlist.data.fd == server_soket){
					connfd=accept();
					addToEpoll(connfd);	// add to connfd socket
				}
				else{
					fp=fopen(“sampleFile”,”r”);
					while(more_data_to_read_from_file){
						// queue a aio read request
						aio_read();
					} 
				}
			}
			
		}
	}
	aioSigHandler(){
		makeHTTPreply();
		send();
		close(socket); // close the connection socket
		closeOpenedFileDesciptors();
	}


Testing with httperf
Testing the program with httperf gives good results if num-conns is 10,000 and rate(number of connections per second) is 1000. 

However for higher amounts of rate, above ~2000, we observer that the program gets stuck in asynchronous IOs.

