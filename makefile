all:
	gcc event_driven_server.c -o server -lrt
	gcc client.c -o client