#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#include "shared.h"

int shmid_for_cleanup = 0;

const int SIZE = 256;

_Bool ready = false;

int create_shm(key_t key, const char *txt, const char *etxt) {
    int shm_id = shmget(key, SIZE, IPC_CREAT | PERM);
    //handle_error(shm_id, etxt, PROCESS_EXIT);
    // printf("%s: shm_id=%d key=%ld\n", txt, shm_id, (long) key);
    return shm_id;
}


void cleanup() {
    if (shmid_for_cleanup > 0) {
        int retcode = shmctl(shmid_for_cleanup, IPC_RMID, NULL);
        shmid_for_cleanup = 0;
        //handle_error(retcode, "removing of shm failed", NO_EXIT);
    }
}

void my_handler(int signo) {
    if (signo == SIGUSR1) {
        printf("received SIGUSR1\n");
        ready = true;
    } else {
        cleanup();
        exit(1);
    }
}

int main(int argc, const char *argv[])
{

    key_t shm_key = ftok(REF_FILE, 1);
    if (shm_key < 0) {
        //handle_error(-1, "ftok failed", PROCESS_EXIT);
    }
    int shm_id = create_shm(shm_key, "main", "main shmget failed");
    //struct data *shm_data = (struct data *) shmat(shm_id, NULL, 0);
    //shmdt(shm_data);
    //shm_data = NULL; /* avoid using the pointer after it has been disabled */

    /* create a worker process */
    pid_t child_pid = fork();
    //handle_error(child_pid, "fork failed", PROCESS_EXIT);

    if (child_pid == 0) {
        //child(shm_key, suspendset_ptr);
        return(0); // done with child
    }
    /* in parent */
    //parent(shm_key, child_pid, suspendset_ptr);
    return 0;
}
