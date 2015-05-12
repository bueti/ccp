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
#include <sys/types.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>

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
board_t board;

pthread_barrier_t start_barrier;

fd_set master;
__thread player_t* player;

void signal_handler(int signo) {
    if(debug) {
        printf("got SIGUSR1 - game can start\n");
        syslog (LOG_DEBUG, "got SIGUSR1 - game can start");
    }

    // TODO: Send to each player the start
    // therefore we need the players list in the board
    //if (send(player->fd, START, sizeof(START), 0) == -1) {
    //    printf("error %i %i\n",player->fd,player->id);
    //    perror("send START");
    //    return NULL;
    //}
    board.game_in_progress = true;
}

/* main */
int main(int argc, char *argv[]) {
    /* Setting up signal handlers */
    signal(SIGUSR1, signal_handler);

    /* Parse command line */
    if(!optparse(argc, argv))
        exit(1);

    /* setup logging */
    setup_logging();
    syslog (LOG_INFO, "---- game server started ----");

    /* Setup tids */
    board.server_tid = pthread_self();

    if(debug) {
        printf("server_tid: %ld\n", (long)board.server_tid);
        syslog (LOG_DEBUG, "server_tid: %ld", (long)board.server_tid);
    }

    board.n = n;
    board.game_in_progress = false;

    /* Setup starting barrier */
    if(pthread_barrier_init(&start_barrier, NULL, board.n/2) != 0) {
        if(debug) {
            printf("pthread_barrier_init failed\n");
        }
        syslog (LOG_ERR, "pthread_barrier_init failed");
        exit(-1);
    }

    /* Allocate cell memory */
    alloc_cell_mem_ptrs(&board);

    if(!init_board(&board)) {
        printf("error initializing board!\n");
        syslog (LOG_ERR, "error initializing board!");
        exit(-1);
    }

    //if(!populate_board(&board)) {
    //    printf("error populating board!\n");
    //    syslog (LOG_ERR, "error populating board!");
    //    exit(-1);
    //}

    /* Create end_checker thread */
    pthread_t end_checker_tid;
    int err = pthread_create(&(end_checker_tid), NULL, &end_checker, NULL);
    if (err != 0) {
        printf("\ncan't create thread :[%s]", strerror(err));
        syslog (LOG_ERR, "error creating thread: [%s]!", strerror(err));
        exit(-1);
    }

    /* create connection handler thread */
    pthread_t connection_handler_tid;
    err = pthread_create(&connection_handler_tid, NULL, connection_handler, NULL);
    if (err != 0) {
        printf("\ncan't create thread :[%s]", strerror(err));
        syslog (LOG_ERR, "error creating thread: [%s]!", strerror(err));
        exit(-1);
    }

    /* wait for threads to finish, close log and exit */
    pthread_join(end_checker_tid, NULL);
    pthread_join(connection_handler_tid, NULL);

    pthread_barrier_destroy(&start_barrier);

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

// see http://www.gnu.org/software/libc/manual/html_node/Server-Example.html
void* connection_handler(void *arg) {
    fd_set read_fds;
    int fdmax;

    int listener;
    int newfd;
    struct sockaddr_storage remoteaddr;
    socklen_t addrlen;

    char remoteIP[INET6_ADDRSTRLEN];

    int yes=1;
    int i,rv;

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
        syslog (LOG_ERR, "listener: failed to bin socket!");
        exit(-1);
    }

    freeaddrinfo(ai); // clean up, not used anymore

    if(debug) {
        printf("listener: waiting to recvfrom...\n");
        syslog (LOG_DEBUG, "listener: waiting to recvfrom...");
    }

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
    read_fds = master;
    if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
        perror("select");
        pthread_exit(0);
    }

    while(true) {
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
                        syslog (LOG_INFO, "server: new connection from %s on socket %d",
                            inet_ntop(remoteaddr.ss_family,
                                get_in_addr((struct sockaddr*)&remoteaddr),
                                remoteIP, INET6_ADDRSTRLEN),
                            newfd);

                        board.num_players++;

                        player_t *new_player;
                        new_player = (player_t *) malloc(sizeof(player_t));

                        new_player->id = i-3;
                        new_player->fd = newfd;

                        char name[255];
                        memset(&name[0], 0, sizeof(name));
                        sprintf(name, "player_%i", new_player->id);
                        strcpy(new_player->name, name);

                        int err = pthread_create(&(new_player->tid), NULL, &game_thread, new_player);
                        if (err != 0)
                            printf("can't create thread :[%s]", strerror(err));

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
            strcpy(board->cells[i][j].player->name, ""); //TODO: player not initialized yet
        }
    }
    return true;
}

void print_board(board_t *board) {
    int i, j;

    for (i=0; i<board->n; i++) {
        for (j=0; j<board->n; j++) {
            printf("%s", board->cells[i][j].player->name);
        }
        printf("\n");
    }
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void* end_checker(void *arg) {
    return NULL;
}

void *game_thread(void *arg) {
    player = (player_t *) arg;

    int nbytes;
    char buf[MAXBUFLEN];

    while(true) {
        int result = (nbytes = recv(player->fd, buf, sizeof buf, 0)) <= 0;

        if( result && nbytes != 0 )
             continue;

        if (result) {
            if (nbytes == 0) {
                // connection closed
                printf("server: player %d with socket %d hung up\n", player->id, player->fd);
            } else {
                perror("recv");
            }

            close(player->fd);
            FD_CLR(player->fd, &master);

            board.num_players--;
    } else {
            if(debug) {
                printf("client %d sent %s\n", player->fd, buf);
                syslog (LOG_INFO, "client %d sent %s, number of players: %d, game in progress: %d", player->fd, buf, board.num_players, board.game_in_progress);
            }
            // Parse incoming data and respond accordingly
            char *res = NULL;
            if(sscanf(buf, HELLO) == 0) {
                // always send the size as a response to HELLO
                if(asprintf(&res, "%s %d\n", SIZE, board.n) == -1)
                    exit(1);

                if(send(player->fd, res, sizeof(res), 0) == -1) {
                    perror("send");
                }
                if(board.game_in_progress) {
                    if (send(player->fd, START, sizeof(START), 0) == -1) {
                        printf("error %i %i\n",player->fd,player->id);
                        perror("send START");
                        return NULL;
                    }
                } else {
                    int retcode = pthread_barrier_wait(&start_barrier);
                    if(retcode == PTHREAD_BARRIER_SERIAL_THREAD) {
                        // send signal here
                        int status = pthread_kill( board.server_tid, SIGUSR1);
                        if ( status <  0) {
                            perror("pthread_kill failed");
                        }
                    } else if(res != 0) {
                        printf("barrier open failed\n");
                        syslog (LOG_DEBUG, "barrier open failed");
                    }

                    // Send Start
                    if (send(player->fd, START, sizeof(START), 0) == -1) {
                        printf("error %i %i\n",player->fd,player->id);
                        perror("send START");
                        return NULL;
                    }
                }
            }

            int x, y;
            int n = sscanf(buf, TAKE, &x, &y);
            if(n == 2) {
                if(take_cell(player, x, y)) {
                    printf("Cell at %d, %d successfully taken!\n", x, y);
                    if (send(player->fd, TAKEN, sizeof(TAKEN), 0) == -1) {
                        perror("send TAKEN");
                    }
                } else {
                    // already taken
                }
            }
        }

    }
    return 0;
}

_Bool take_cell(player_t *player, int x, int y) {
    return true;
}
