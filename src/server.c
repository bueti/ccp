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
board_t *board;
fd_set master;
__thread player_t* player;

/* Barrier to synchronize START command */
pthread_barrier_t start_barrier;

/* Signal Handler used to start the game when enough players connected */
void signal_handler(int signo) {
    if(signo == SIGUSR1) {
        if(debug) {
            printf("Caught SIGUSR1 - game can start\n");
            syslog (LOG_DEBUG, "Caught SIGUSR1 - game can start");
        }
        board->game_in_progress = true;
    }
    if(signo == SIGINT) {
        printf("Caught SIGINT, cleaning up and exiting...\n");
        syslog (LOG_INFO, "Caught SIGINT - cleaning up and exiting...");
        cleanup();
    }
}

/* main */
int main(int argc, char *argv[]) {
    /* Setting up signal handlers */
    signal(SIGUSR1, signal_handler);
    signal(SIGINT, signal_handler);

    /* Parse command line */
    if(!optparse(argc, argv))
        exit(1);

    /* setup logging */
    setup_logging();
    syslog (LOG_INFO, "---- game server started ----");

    /* Allocate cell memory */
    board = malloc(sizeof(board_t));
    board->cells = (cell_t *) malloc(sizeof(cell_t)*n*n);
    board->cells->player = (player_t*) malloc(sizeof(player_t));

    player_t* dummy  = (player_t *) malloc(sizeof(player_t));
    dummy->name  = malloc(sizeof(char)*256);
    dummy->name = "free";
    for(int i = 0; i < n*n; i++){
        board->cells[i].player = dummy;
        pthread_mutex_init(&board->cells[i].cell_mutex, NULL);
    }

    /* Setup board basics */
    board->server_tid = pthread_self();
    board->checker_tid = NULL;
    board->n = n;
    board->game_in_progress = false;
    board->num_players = 0;

    if(debug) {
        printf("server_tid: %ld\n", (long)board->server_tid);
        syslog (LOG_DEBUG, "server_tid: %ld", (long)board->server_tid);
        printf("board size: %d\n", board->n);
        syslog (LOG_DEBUG, "board size: %d", board->n);
    }

    /* Setup starting barrier */
    if(pthread_barrier_init(&start_barrier, NULL, board->n/2) != 0) {
        if(debug) {
            printf("pthread_barrier_init failed\n");
        }
        syslog (LOG_ERR, "pthread_barrier_init failed");
        exit(-1);
    }

    /* create connection handler thread */
    pthread_t connection_handler_tid;
    int err = pthread_create(&connection_handler_tid, NULL, connection_handler, NULL);
    if (err != 0) {
        printf("\ncan't create thread :[%s]", strerror(err));
        syslog (LOG_ERR, "error creating thread: [%s]!", strerror(err));
        exit(-1);
    }

    /* wait for threads to finish, close log and exit */
    pthread_join(board->checker_tid, NULL);
    pthread_join(connection_handler_tid, NULL);

    cleanup();

    return 0;
}

void cleanup() {
    pthread_barrier_destroy(&start_barrier);

    for(int i = 0; i < n*n; i++){
        pthread_mutex_destroy(&board->cells[i].cell_mutex);
    }

    free(board->cells);
    free(board);

    syslog (LOG_INFO, "---- game server stopped ----");
    closelog();
    exit(0);
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
void* connection_handler() {
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

                        board->num_players++;

                        player_t *new_player;
                        new_player = (player_t *) malloc(sizeof(player_t));
                        new_player->name = malloc(sizeof(char)*256);

                        new_player->id = newfd-4;
                        new_player->fd = newfd;

                        char name[255];
                        memset(&name[0], 0, sizeof(name));
                        sprintf(name, "player_%i", new_player->id);
                        strcpy(new_player->name, name);

                        int err = pthread_create(&(new_player->tid), NULL, &game_thread, new_player);
                        if (err != 0) {
                            printf("can't create thread :[%s]", strerror(err));
                            if(send(player->fd, NACK, sizeof(NACK), 0) == -1) {
                                perror("ERROR sending NACK");
                            }
                        }

                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END while

    return 0;
}

_Bool init_board() {

    return true;
}

void print_board() {
    int i;

    for (i=0; i<board->n; i++) {
        printf("%s", board->cells[i].player->name);
    }
}

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void* end_checker() {
    if(debug) {
        printf("Starting end_checker\n");
    }
    syslog(LOG_INFO, "starting end_checker thread: %ld", (long)board->checker_tid);

    while(true) {
        // wait a random amount of time http://stackoverflow.com/questions/822323/how-to-generate-a-random-number-in-c
        srand(time(NULL));
        int sleep_time = rand() % 10 + 1;
        if(debug)
            printf("sleeping for %d\n", sleep_time);
        sleep(sleep_time);

        int counter = 0;
        // loop though board, check state, if all taken, end game
        if(debug) {
            printf("looping through board\n");
            syslog(LOG_DEBUG, "looping through board");
        }

        for(int i=0; i<board->n*board->n; i++) {
            if(pthread_mutex_trylock(&board->cells[i].cell_mutex) != 0) {
                //taken
                if(debug) {
                    printf("%d taken by %s\n", i, board->cells[i].player->name);
                    syslog(LOG_DEBUG, "%d taken by %s", i, board->cells[i].player->name);
                }
                counter++;
            } else {
                if(pthread_mutex_unlock(&board->cells[i].cell_mutex) != 0) {
                    printf("error while unlocking\n");
                    syslog(LOG_ERR, "error while unlocking");
                } else {
                    if(debug) {
                        printf("mutex not locked -> cell not taken\n");
                        syslog(LOG_DEBUG, "mutex not locked -> cell not  taken");
                    }
                }
            }
        }
        if(debug) {
            syslog(LOG_DEBUG, "mutex not locked -> cell not  taken");
            printf("finished looping through board\n");
        }

        if(counter >= board->n*board->n) {
            printf("game over...\n");
            // TODO: Send END to all clients
            exit(0);
        }
    }
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
                syslog(LOG_INFO, "server: player %d with socket %d hung up\n", player->id, player->fd);
            } else {
                perror("recv");
            }

            close(player->fd);
            FD_CLR(player->fd, &master);

            board->num_players--;
    } else {
            // Parse incoming data and respond accordingly
            char *res = NULL;

            // Remove \r\n from buffer. \r\n is added by telnet when manually talk to the server
            char *data = strip_copy(buf);

            if(debug) {
                printf("client %d sent %s\n", player->fd, data);
                syslog (LOG_INFO, "client %d sent %s, number of players: %d, game in progress: %d", player->fd, data, board->num_players, board->game_in_progress);
            }

            if(strcmp(data, HELLO) == 0) {
                // always send the size as a response to HELLO
                if(asprintf(&res, "%s %d\n", SIZE, board->n) == -1)
                    exit(1);

                if(send(player->fd, res, sizeof(res), 0) == -1) {
                    perror("send");
                }
                if(board->game_in_progress) {
                    if (send(player->fd, START, sizeof(START), 0) == -1) {
                        printf("error %i %i\n",player->fd,player->id);
                        perror("send START");
                        return NULL;
                    }
                } else {
                    int retcode = pthread_barrier_wait(&start_barrier);
                    if(retcode == PTHREAD_BARRIER_SERIAL_THREAD) {
                        // Notify all players
                        int status = pthread_kill( board->server_tid, SIGUSR1);
                        if ( status <  0) {
                            perror("Sending Start Signal to server_tid failed");
                        }

                        /* Create end_checker thread */
                        pthread_t end_checker_tid;
                        int err = pthread_create(&(end_checker_tid), NULL, &end_checker, NULL);
                        if (err != 0) {
                            printf("\ncan't create thread :[%s]", strerror(err));
                            syslog (LOG_ERR, "error creating thread: [%s]!", strerror(err));
                            exit(-1);
                        }
                        board->checker_tid = end_checker_tid;

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
            } else if (strncmp(buf, TAKE, 4) == 0) {
                if(debug) {
                    printf("TAKE: client %d sent %s\n", player->fd, data);
                    syslog (LOG_DEBUG, "client %d sent %s", player->fd, data);
                }
                int x, y;
                char cmd[20];
                int n = sscanf(data, "%s %d %d", cmd, &x, &y);

                if(n == 3) {
                    if(take_cell(player, x, y)) {
                        printf("Cell at %d, %d successfully taken!\n", x, y);
                        if (send(player->fd, TAKEN, sizeof(TAKEN), 0) == -1) {
                            perror("send TAKEN");
                        }
                    } else {
                        // already taken
                        printf("Cell at %d, %d already taken!\n", x, y);
                        if (send(player->fd, INUSE, sizeof(TAKEN), 0) == -1) {
                            perror("send INUSE");
                        }
                    }
                }
            }
            // TODO: Implement properly
            else if (strncmp(buf, STATUS, 4) == 0) {
                if(debug) {
                    printf("STATUS: client %d sent %s\n", player->fd, data);
                    syslog (LOG_DEBUG, "client %d sent %s", player->fd, data);
                }
                int x, y;
                char cmd[20];
                int n = sscanf(data, "%s %d %d", cmd, &x, &y);
                char *res = "NOT IMPLEMENTED";
                if (send(player->fd, res, sizeof(res), 0) == -1) {
                    perror("send STATUS");
                }
            } else {
                char *res = "NACK";
                if (send(player->fd, res, sizeof(res), 0) == -1) {
                    perror("send UNKNOWN");
                }

            }
        }

    }
    return 0;
}

_Bool take_cell(player_t *player, int x, int y) {
    int cell_id = board->n * x + y;
    if(cell_id > board->n*board->n) {
        printf("out of range\n");
        return false;
    }
    if(pthread_mutex_trylock(&board->cells[cell_id].cell_mutex) == 0) {
        // successfully locked
        board->cells[cell_id].player->name = player->name;
        pthread_mutex_unlock(&board->cells[cell_id].cell_mutex);
        return true;
    } else {
        // already locked
        return false;
    }
}

// from http://stackoverflow.com/questions/1515195/how-to-remove-n-or-t-from-a-given-string-in-c
char *strip_copy(const char *s) {
    char *p = malloc(strlen(s) + 1);
    if(p) {
        char *p2 = p;
        while(*s != '\0') {
            if(*s != '\r' && *s != '\n') {
                *p2++ = *s++;
            } else {
                ++s;
            }
        }
        *p2 = '\0';
    }
    return p;
}
