/*
* structs
*/

typedef struct {
    char * name;
} player_t;

typedef struct {
    int fd;
    int size;
} client_t;

/*
* functions
*/
void *get_in_addr(struct sockaddr *sa);
int start_client();
void start_simple(int sockfd);

