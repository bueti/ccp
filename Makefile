server:
	gcc -g server.c server_optparse.c -o server
client:
	gcc -g client.c client_optparse.c -o client
all:
	gcc -g server.c server_optparse.c -o server
	gcc -g client.c client_optparse.c -o client
