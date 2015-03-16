server:
	gcc -Ilib -g server.c server_optparse.c -o server
client:
	gcc -Ilib -g client.c client_optparse.c -o client
all:
	gcc -Ilib -g server.c server_optparse.c -o server
	gcc -Ilib -g client.c client_optparse.c -o client
