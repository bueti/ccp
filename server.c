#include <stdlib.h>
#include <stdio.h>

#include "server_optparse.h"

/*
* Global Vars
*/
int n = 0;

/*
* My Structs
*/
typedef struct {
    int pos_x;
    int pos_y;
    _Bool occupied;
} cell_t;

typedef struct {
    char * name;
} player_t;

typedef struct {
    int id;
    player_t player;
} players_t;

int main(int argc, char *argv[])
{
     // Parse command line
    if(!optparse(argc, argv))
        exit(1);

    printf("size is: %d\n", n);
    return 0;
}
