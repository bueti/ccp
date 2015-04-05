#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <syslog.h>

#include "server.h"
#include "server_optparse.h"
#include "commands.h"
#include "shared.h"

#define PORT "1666"
#define BACKLOG 10
#define MAXBUFLEN 256
#define MAXPLAYERS 50

/* Global Vars */
int n = 0;
_Bool debug = false;

/* main */
int main(int argc, char *argv[]) {
    /* Parse command line */
    if(!optparse(argc, argv))
        exit(1);

    /* setup logging */
    setup_logging();
    syslog (LOG_INFO, "---- game server started ----");

    /* Setup pids */
    pid_t child_pid, server_pid;
    server_pid = getpid();

    if(debug) {
        printf("server_pid: %d\n", server_pid);
        syslog (LOG_DEBUG, "server_pid: %d", server_pid);
    }

    /* create shared memory object */
    int shm_fd;
    void *ptr;
    char *shm_name = "/board";

    shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, PERM);

    board_t board;
    board.n = n;

    /* Allocate cell memory */
    alloc_cell_mem_ptrs(&board);

    if(!init_board(&board)) {
        printf("error initializing board!\n");
        syslog (LOG_ERR, "error initializing board!");
        exit(-1);
    }

    if(!populate_board(&board)) {
        printf("error populating board!\n");
        syslog (LOG_ERR, "error populating board!");
        exit(-1);
    }

    /* truncate shared memory to size of board */
    size_t board_size = sizeof(&board);

    if((ftruncate(shm_fd, board_size)) != 0) {
        perror("ftruncate");
    }

    /* now map the shared memory segment in the address space of the process */
    ptr = mmap(0, board_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (ptr == MAP_FAILED) {
        printf("Map failed\n");
        syslog (LOG_ERR, "Map failed");
        exit(1);
    }
    syslog (LOG_DEBUG, "shm_mem size: %ld", board_size);

    /* Create end_checker fork */
    child_pid = fork();
    if(child_pid == 0) {
        syslog (LOG_DEBUG, "end_checker_pid: %d", getpid());
        data_t data;
        data.shm_fd = shm_fd;

        end_checker(&data);
    } else {

        /* Still in parent - open connection handler */
        child_pid = fork();
        if(child_pid == 0) {
            syslog (LOG_DEBUG, "connection_handler_pid: %d", getpid());
            _Bool retcode = wait_for_connections(&board); // that doesnt work... please implement! (TODO)
        }
    }

    /* wait for children to finish, close log and exit */
    int status;
    wait(&status);
    syslog (LOG_INFO, "---- game server stopped ----");
    closelog();

    return 0;
}

void setup_logging() {
    if(debug) {
        setlogmask (LOG_UPTO (LOG_DEBUG));
    } else {
        setlogmask (LOG_UPTO (LOG_NOTICE));
    }
    openlog ("game_server", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
}

_Bool wait_for_connections(board_t *board) {

    pthread_t game_tid[MAXPLAYERS];

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
            data_t data;
            data.fd = i;
            data.id = i-3;

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
                        if(debug)
                            printf("client %d sent %s\n", i, buf);

                        if(strcmp(buf, HELLO)) {
                            // always send the size as a response to HELLO
                            char *res = NULL;
                            if(asprintf(&res, "%s %d\n", SIZE, board->n) == -1)
                                exit(1);

                            if(send(i, res, sizeof(res), 0) == -1) {
                                perror("send");
                            }

                            // send start when we have n/2 connections,
                            // otherwise send it only to the new guy
                            // and start a new game thread
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

                                int err = pthread_create(&(game_tid[data.id]), NULL, &game_thread, &data);
                                if (err != 0)
                                    printf("can't create thread :[%s]", strerror(err));
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

    if(debug) {
        printf("board size: %d\n", board->n);
        syslog (LOG_DEBUG, "board size: %d", board->n);
    }

    return true;
}

_Bool populate_board(board_t *board) {
    int i, j;
    for (i = 0; i<board->n; i++) {
        for (j=0; j<board->n; j++) {
            strcpy(board->cells[i][j].owner, "");
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

void *game_thread(data_t *data) {
    if(debug)
        printf("In game thread. fd: %d, id: %d\n", data->fd, data->id);

    return 0;
}

void end_checker(data_t *data) {
    if(debug)
        printf("in end_checker. shm_id: %d\n", data->shm_fd);
    while(true) {

    }
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

