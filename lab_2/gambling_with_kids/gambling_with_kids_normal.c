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
#include <time.h>

// Scrieți un program C în care un proces tată creează un proces fiu și verifică paritatea PID-ului fiului. În funcție de rezultat, tatăl transmite fiului unul dintre mesajele: 
//     "fortune" – dacă PID-ul fiului este par,
//     "lost" – dacă PID-ul fiului este impar (in acest caz va muri înaintea fiului)
// Implementați comunicarea între procese în trei variante distincte, conform situațiilor de mai jos:
//     S1. Tatăl și fiul cunosc numele fișierului (ex. mesaj.txt); Tatăl scrie în fișier mesajul corespunzător parității PID-ului fiului; Fiul așteaptă (prin sleep()

void child_stuff() {
    struct stat st;
    while (stat("message.txt", &st) == -1 || st.st_size == 0) { usleep(1000); }

     FILE *child_support;
    if ((child_support = fopen("message.txt", "r")) == NULL) {
        perror("fopen(child)");
        exit(1);
    }

    char line [128];
    while (fgets(line, sizeof(line), child_support)) {
        printf("%s\n", line);    
    }
    remove("message.txt");
    fclose(child_support);
}

void parent_stuff(pid_t pid) {
    FILE* gambling = fopen("message.txt", "w");
    if (!gambling) {
        perror("fopen(parent)");
        exit(1);
    }
    printf("%d\n", pid);

    if(pid % 2 == 0) {
        fprintf(gambling, "treasure");
        fclose(gambling);
    }
    else {
        fclose(gambling);
        remove("message.txt");
        exit(1);
        fprintf(gambling, "lost");
    }
}

int main(int argc, char* argv[]) {
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
    return 0;
}