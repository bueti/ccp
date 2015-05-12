/*
 * structs
 */
typedef struct {
    char * name;
    int id;
    int fd;
    pthread_t tid;
} player_t;

typedef struct {
    int pos_x;
    int pos_y;
    pthread_mutex_t mutex;
    player_t * player;
} cell_t;

typedef struct {
    int n;
    int num_players;
    pthread_t server_tid;
    _Bool game_in_progress;
    cell_t **cells;
} board_t;

typedef struct {
    int fd;
    int shm_fd;
    int id;
    char* buf;
} data_t;

/*
* functions
*/
void sigchld_handler(int s);
void *get_ind_addr(struct sockaddr);
int start_server();
void alloc_cell_mem_ptrs(board_t *board);
_Bool init_board(board_t *board);
_Bool populate_board(board_t *board);
void print_board(board_t *board);
_Bool handle_incoming(int fd);
_Bool *read_cell(board_t *board, int x, int y);
void *get_in_addr(struct sockaddr *sa);
_Bool take_cell(player_t *player, int x, int y);
char *strip_copy(const char *s);
// Thread runs
void* end_checker(void *arg);
void* connection_handler(void *arg);
void *game_thread(void *arg);
