#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "shared.h"

int sempid_for_cleanup = 0;

int create_sem(key_t key, const char *txt, const char *etxt) {
    int semaphore_id = semget(key, 1, IPC_CREAT | PERM);
    //handle_error(semaphore_id, etxt, PROCESS_EXIT);
    printf("%s: semaphore_id=%d key=%ld\n", txt, semaphore_id, (long) key);
    fflush(stdout);
    return semaphore_id;
}

void cleanup() {
    if (sempid_for_cleanup > 0) {
        int retcode = semctl(sempid_for_cleanup, 0, IPC_RMID, NULL);
        sempid_for_cleanup = 0;
        //handle_error(retcode, "removing of semaphore failed", PROCESS_EXIT);
    }
}

void my_handler(int signo) {
    cleanup();
    exit(1);
}
