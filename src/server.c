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

#include "server.h"
#include "server_optparse.h"

#define PORT "1666"
#define BACKLOG 10
#define MAXBUFLEN 255

/*
* Global Vars
*/
int n = 0;

/**
 * Signal Handler
 */
void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
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

    if(!populate_board(&board)) {
        printf("error populating board!\n");
        exit(-1);
    }

    print_board(&board);
    start_server();


    return 0;
}

int start_server() {
    // from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    char buf[MAXBUFLEN];
    int numbytes;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                get_in_addr((struct sockaddr *)&their_addr),
                s, sizeof s);

        printf("server: got packet from %s\n",
                inet_ntop(their_addr.ss_family,
                        get_in_addr((struct sockaddr *)&their_addr),
                        s, sizeof s));
        printf("listener: packet is %d bytes long\n", numbytes);
        buf[numbytes] = '\0';
        printf("listener: packet contains \"%s\"\n", buf);

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            //char msg[] = "Hello to the coolest game ever!\n";
            //if (send(new_fd, msg, sizeof(msg), 0) == -1)
            //    perror("send");
            if(handle_incoming(new_fd)) {
                // successfull
                close(new_fd);
                exit(0);
            } else {
               // error handling
                close(new_fd);
                exit(-1);
            }
        }
        close(new_fd);  // parent doesn't need this
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
    printf("size is: %d\n", board->n);

    return true;
}

void alloc_cell_mem_ptrs(board_t *board) {
    board->cells = (cell_t**)malloc(board->n * sizeof(cell_t*));

    int i;
    for (i = 0; i < board->n; ++i) {
        board->cells[i] = (cell_t*)malloc((board->n * sizeof(cell_t*)));
    }

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
    return true;
}
