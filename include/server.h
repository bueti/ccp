/*
* My Structs
*/
typedef struct {
    int pos_x;
    int pos_y;
    pthread_rwlock_t lock;
    char owner[]; // TODO: should be player_t
} cell_t;

typedef struct {
    int n;
    cell_t **cells;
} board_t;

typedef struct {
    char * name;
} player_t;

typedef struct {
    int id;
    player_t **player;
} players_t;

/*
* My functions
*/
void sigchld_handler(int s);
void *get_ind_addr(struct sockaddr);
int start_server();
void alloc_cell_mem_ptrs(board_t *board);
_Bool init_board(board_t *board);
_Bool populate_board(board_t *board);
void print_board(board_t *board);
_Bool handle_incoming(int fd);

