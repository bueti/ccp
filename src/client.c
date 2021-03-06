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
#include "client.h"
#include "commands.h"
#include "shared.h"

#define MAXDATASIZE 256 // max number of bytes we can get at once
#define PORT "1666"
#define HOST "localhost"

/*
* Global Vars
*/
char *name;
char *hostname;
char *port;
int size;

int main(int argc, char *argv[]) {
    hostname = HOST;
    port = PORT;

    // Parse command line
    if(!optparse(argc, argv))
        exit(1);

    player_t* player;

    player = malloc(sizeof(player_t));
    player->name = name;

    printf("Player Name: %s\n", player->name);

    int retcode = start_client(player);

    return retcode;
}

int start_client(player_t *player) {
    // from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "failed: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    char *msg = "HELLO";
    if (send(sockfd, msg, strlen(msg), 0) == -1) {
        perror("client: sendto");
        close(sockfd);
        exit(-1);
    }
    else {
        numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
        if (numbytes <= 0) {
            perror("recv");
            exit(-1);
        }

        printf("Server answer: %s\n", buf);
        char* s;
        char* answer;
        s = strtok(buf, "\0");
        answer = strtok(s, "\n");

        char cmd[20];
        int n = sscanf(answer, "%s %d", cmd, &size);
        if(n<2)
            exit(-1);

        if(strncmp(cmd, SIZE, 4) != 0) {
            printf("Server answered with wrong command, aborting...\n");
            exit(-1);
        }

    }
    while(true) {
        numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0);
        if (numbytes <= 0) {
            perror("recv");
            exit(-1);
        }

        printf("Server answer: %s\n", buf);
        if(strncmp(buf, START, 4) == 0) {
            start_simple(sockfd, player);
        }

    }

    close(sockfd);

    return 0;
}

/*
 * simplest implementation of a client
 * will go through 1 to n and try to fetch the cell
 */
void start_simple(int fd, player_t *player) {
    char *msg = NULL;
    char buf[MAXDATASIZE];
    int numbytes;
    _Bool isRunning = true;
    int maxTries = 10;
    int counter = 0;

    while(isRunning || counter<maxTries) {
        for (int i=0; i<size; i++) {
            for (int j=0; j<size; j++) {
                int ret = asprintf(&msg, "%s %d %d %s\n", TAKE, i, j, player->name);
                if(ret == -1)
                    printf("asprintf failed\n");

                printf("Sending %s to server\n", msg);

                if (send(fd, msg, strlen(msg), 0) == -1) {
                    perror("client: sendto");
                    close(fd);
                    exit(-1);
                }

                numbytes = recv(fd, buf, MAXDATASIZE-1, 0);
                if (numbytes <= 0) {
                    perror("recv");
                    exit(-1);
                }

                if(strncmp(buf, END, 3) == 0) {
                    printf("Game is over\n");
                    exit(0);
                }

                if(!strncmp(buf, TAKEN, 5) == 0) {
                    // retry
                    if (send(fd, msg, strlen(msg), 0) == -1) {
                        perror("client: sendto");
                        close(fd);
                        exit(-1);
                    }
                }
                printf("2nd try - server answer: %s\n", buf);
                if(strncmp(buf, END, 3) == 0) {
                    printf("Game is over\n");
                    isRunning = false;
                }
            }
        }
        counter++;
    }
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
