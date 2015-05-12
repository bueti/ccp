#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "client_optparse.h"

/*
* My Structs
*/

typedef struct {
    char * name;
} player_t;

/*
* Global Vars
*/
char *name;
char *hostname;
char *port;

/*
* My functions
*/
int start_client();


int main(int argc, char *argv[]) {
    // Parse command line
    if(!optparse(argc, argv))
        exit(1);

    player_t player;
    player.name = name;

    printf("Player Name: %s\n", player.name);

    int retcode = start_client(player);

    return retcode;
}

int start_client(player_t player) {
    // from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("talker: socket");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "talker: failed to bind socket\n");
        return 2;
    }

    char *msg = "HELLO";
    if ((numbytes = sendto(sockfd, msg, strlen(msg), 0, p->ai_addr, p->ai_addrlen)) == -1) {
        perror("client: sendto");
        exit(1);
    }

    freeaddrinfo(servinfo);

    printf("client: sent %d bytes to %s\n", numbytes, msg );

    _Bool received = false;

    while(!received) {

    }

    close(sockfd);

    return 0;
}

