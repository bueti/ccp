/*
 * structs
 */
typedef struct {
    char *name;
    int id;
    int fd;
    pthread_t tid;
    int num_of_cells;
} player_t;

typedef struct {
    pthread_mutex_t cell_mutex;
    player_t *player;
} cell_t;

typedef struct {
    int n;
    int num_players;
    pthread_t server_tid;
    pthread_t checker_tid;
    _Bool game_in_progress;
    cell_t *cells;
} board_t;

/*
* functions
*/
void *get_in_addr(struct sockaddr *sa);
void cleanup();
void print_board();
_Bool take_cell(player_t *player, int x, int y);
char *strip_copy(const char *s);
// Thread runs
void *end_checker();
void *connection_handler();
void *game_thread(void *arg);
