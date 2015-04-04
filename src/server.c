#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#include "server.h"
#include "server_optparse.h"
#include "commands.h"

#define PORT "1666"
#define BACKLOG 10
#define MAXBUFLEN 256

/*
* Global Vars
*/
int n = 0;
_Bool debug = false;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[]) {
    // Parse command line
    if(!optparse(argc, argv))
        exit(1);

    board_t board;
    board.n = n;

    /* Allocate cell memory */
    alloc_cell_mem_ptrs(&board);

    if(!init_board(&board)) {
        printf("error initializing board!\n");
        exit(-1);
    }

//    if(!populate_board(&board)) {
//        printf("error populating board!\n");
//        exit(-1);
//    }

    if(debug)
        print_board(&board);

    _Bool retcode = wait_for_connections(&board);

    return retcode;
}

_Bool wait_for_connections(board_t *board) {
    _Bool started = false;

    fd_set master;
    fd_set read_fds;
    int fdmax;

    int listener;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;

    char buf[MAXBUFLEN];
    int nbytes;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;
    int i,j,rv;

    struct addrinfo hints, *ai, *p;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    // get a socket and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // set to AF_INET to force IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &ai)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    // loop through all the results and bind to the first we can
    for(p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(listener < 0) {
            continue;
        }

        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }

        break;
    }

    // if we get here we cloudnt bind to a socket
    if (p == NULL) {
        fprintf(stderr, "listener: failed to bind socket\n");
        exit(2);
    }

    freeaddrinfo(ai); // clean up, not used anymore

    printf("listener: waiting to recvfrom...\n");

    // listen
    if(listen(listener, BACKLOG) == -1) {
        perror("listen");
        exit(3);
    }

    // add listener to the master set
    FD_SET(listener, &master);

    // keep track of the biggest fd
    fdmax = listener;


    // main loop
    while(true) {
        read_fds = master;
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(4);
        }

        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) { // we got one!!
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof remoteaddr;
                    newfd = accept(listener,
                        (struct sockaddr *)&remoteaddr,
                        &addrlen);

                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("server: new connection from %s on "
                            "socket %d\n",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);
                    }
                } else {
                    // handle data from a client
                    if ((nbytes = recv(i, buf, sizeof buf, 0)) <= 0) {
                        // got error or connection closed by client
                        if (nbytes == 0) {
                            // connection closed
                            printf("server: socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    } else {
                        // we got some data from a client
                        if(debug) {
                            printf("client %d sent %s\n", i, buf);
                        }
                        if(strcmp(buf, HELLO)) {
                            // always send the size as a response to HELLO
                            char *res = NULL;
                            if(asprintf(&res, "%s %d\n", SIZE, board->n) != 0)
                                exit(1);

                            if(send(i, res, sizeof(res), 0) == -1) {
                                perror("send");
                            }

                            // send start when we have n/2 connections,
                            // otherwise send it only to the new guy
                            if((fdmax-3) >= board->n/2 && !started) {

                                for(j = 0; j <= fdmax; j++) {
                                    // send to everyone!
                                    if (FD_ISSET(j, &master)) {
                                        // except the listener
                                        if (j != listener) {
                                            if (send(j, START, sizeof(START), 0) == -1) {
                                                perror("send");
                                            }
                                        }
                                    }
                                }
                                started = true;
                            } else if ((fdmax-3) >= board->n/2) {
                                if(send(i, START, sizeof(START), 0) == -1) {
                                    perror("send");
                                }
                            }
                        }
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while

    return 0;
}


void alloc_cell_mem_ptrs(board_t *board) {
    board->cells = (cell_t**)malloc(board->n * sizeof(cell_t*));

    int i;
    for (i = 0; i < board->n; ++i) {
        board->cells[i] = (cell_t*)malloc(board->n * sizeof(cell_t*));
    }

}

_Bool init_board(board_t *board) {
    // allocate memory for the data of all cells on the board
    board->cells = (cell_t **) malloc(sizeof (cell_t *) * board->n);
    int i;
    for (i = 0; i < board->n ; ++i) {
       if(!(board->cells[i] = (cell_t *) malloc(sizeof(cell_t *) * board->n)))
            return false;
    }

    if(debug)
        printf("size is: %d\n", board->n);

    return true;
}

_Bool populate_board(board_t *board) {
    int i, j;
    for (i = 0; i<board->n; i++) {
        for (j=0; j<board->n; j++) {
            strcpy(board->cells[i][j].owner, "");
            int retcode = pthread_rwlock_init(&board->cells[i][j].lock, NULL);
            if (retcode != 0) {
                printf("%d/%d: mutex init failed\n", i, j);
                return 1;
            }
        }
    }
    return true;
}

void print_board(board_t *board) {
    int i, j;

    for (i=0; i<board->n; i++) {
        for (j=0; j<board->n; j++) {
            printf("%s", board->cells[i][j].owner);
        }
        printf("\n");
    }
}

_Bool handle_incoming(int fd) {
    int numbytes;
    char buf[MAXBUFLEN];
    if ((numbytes = recv(fd, buf, MAXBUFLEN-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }

    printf("%s\n", buf);
    if (send(fd, COMMANDS[0], sizeof(COMMANDS[0]), 0) == -1)
        perror("send");
    return true;
}


_Bool *read_cell(board_t *board, int x, int y) {

    if (pthread_rwlock_rdlock(&board->cells[x][y].lock) < 0) {
        perror("rdlock");
    } else {
        strcpy(board->cells[x][y].owner, "foo");
        if (pthread_rwlock_unlock(&board->cells[x][y].lock) != 0) {
            perror("reader thread: pthred_rwlock_unlock error");
            exit(__LINE__);
        }
    }
    return false; // if we cant lock the cell it's already taken
}


char* concat(char *s1, char *s2) {
    char *result = malloc(strlen(s1)+strlen(s2)+1);//+1 for the zero-terminator
    //in real code you would check for errors in malloc here
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}
