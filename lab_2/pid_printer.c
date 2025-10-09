#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <limits.h>
#include <fnmatch.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

// Scrieți un program care:
// Afișează din 3 în 3 secunde PID-ul procesului curent și numărul curent al afișării.
// La primirea semnalului SIGUSR1, programul scrie în fișierul “semnal.txt” un mesaj
// Semnalul SIGINT (Ctrl + C) trebuie:
// ignorat în primele 60 de secunde de la pornirea programului;
// revenit la acțiunea implicită (default) după acest interval.


void int_handler(int signal) {}

void alrm_handler(int signa) {
    signal(SIGINT, SIG_DFL);

}

void usr1_handler(int signal) {
    FILE* file;
    if((file = fopen("signal.txt", "a")) == NULL) {
        perror("Error at fopen");
        return;
    }

    fprintf(file, "Pokeristii was here.\n");
    fclose(file);
}

int main(int argc, char* argv[]){
    pid_t pid = getpid();
    int count = 0;
    remove("signal.txt");

    signal(SIGINT, int_handler);
    signal(SIGALRM, alrm_handler);
    signal(SIGUSR1, usr1_handler);

    alarm(60);

    while(true) {
        count++;
        printf("%d: %d\n",count, pid);
        sleep(3);
    }


}