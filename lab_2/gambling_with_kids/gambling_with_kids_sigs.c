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

// Scrieți un program C în care un proces tată creează un proces fiu și verifică paritatea PID-ului fiului. În funcție de rezultat, tatăl transmite fiului unul dintre mesajele: 
//     "fortune" – dacă PID-ul fiului este par,
//     "lost" – dacă PID-ul fiului este impar (in acest caz va muri înaintea fiului)
// Implementați comunicarea între procese în trei variante distincte, conform situațiilor de mai jos:
//     S3. Tatăl și fiul comunică exclusiv prin semnale UNIX (SIGUSR1, SIGUSR2, etc.). Tatăl trimite fiului:
//             SIGUSR1 dacă PID-ul fiului este par (semnificând „fortune”);
//             SIGUSR2 dacă PID-ul fiului este impar (semnificând „lost”).
//             Fiul instalează rutine de tratare a semnalelor (sigaction/signal) și afișează pe ecran mesajul corespunzător semnalului primit.

void usr1_handler(int sig) {
    printf("fortune.\n");
    exit(0);
}

void usr2_handler(int sig) {
    printf("lost.\n");
    exit(0);
}

void child_stuff() {
    while(true) {};
}

void parent_stuff(pid_t pid) {
    printf("%d\n", pid);

    if(pid % 2 == 0) {
        kill(pid, SIGUSR1);
    }
    else {
        kill(pid, SIGUSR2);
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    signal(SIGUSR1, usr1_handler);
    signal(SIGUSR2, usr2_handler);

    if(argc > 1) {
        printf("Too many arguments.");
        exit(1);
    }

    pid_t pid;

    if((pid = fork()) == -1) {
        perror("Eroare la fork");
        exit(2);
    }

    if(pid == 0) {
        child_stuff();
        return 0;
    }

    parent_stuff(pid);

    wait(NULL);
    return 0;
}