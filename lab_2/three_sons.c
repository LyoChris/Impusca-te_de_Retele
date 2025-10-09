#define _POSIX_C_SOURCE 200809L
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

// Se scrie un program în care un proces tată creează doi fii, cu următoarele roluri:
// Primul fiu (F1):
// Verifică utilizatorii logați în sistem și scrie rezultatul într-un fișier useri.txt. (aici puteti rula orice comanda)
// Al doilea fiu (F2):
// Citește conținutul fișierului useri.txt generat de F1.
// Filtrează rezultatele după o anumită locație de logare (de exemplu: „tty1”, „pts/2” etc.).
// Afișează pe ecran rezultatul filtrat sau îl trimite procesului tată (după implementare).
// Mecanism de sincronizare:
// Primul fiu, după ce a terminat scrierea fișierului, trimite un semnal (SIGUSR1) celui de-al doilea fiu pentru a-l anunța că datele sunt disponibile.
// Tatăl poate folosi wait() pentru a aștepta finalizarea ambilor fii și a afișa un mesaj de confirmare finală.

void handle_usr1(int signal) {}

void fiu2_stuff(int fd, sigset_t *oldmask) {
    if (signal(SIGUSR1, handle_usr1) == SIG_ERR) {
        perror("signal(SIGUSR1) in F2");
    }

    sigsuspend(oldmask);

    FILE *file;
    if ((file = fopen("users.txt", "r")) == NULL) { 
        perror("fopen useri.txt"); 
        exit(3); 
    }

    char *line = NULL;
    size_t cap = 0;
    ssize_t n;

    while ((n = getline(&line, &cap, file)) != -1) {
        // if (strstr(line, "pts/") != NULL) {
        //     if (write(fd, line, n) < 0) {
        //         perror("write to parent");
        //         break;
        //     }
        // }
        if (strstr(line, "tty") != NULL) {
            if (write(fd, line, n) < 0) {
                perror("write to parent");
                break;
            }
        }
    }
    free(line);
    fclose(file);
    close(fd);
    exit(0);
}

void fiu1_stuff(pid_t pid2) {
    pid_t gc;
    if((gc = fork()) == -1) {
        perror("Eroare la fork grand children");
        exit(1);
    }

    if(gc == 0) {
        int fd;
        if((fd = open("users.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)) == -1) {
            perror("open useri.txt");
            exit(5);
        }

        if(dup2(fd, STDOUT_FILENO) == -1) {
            perror("dup2");
            exit(6);
        }

        close(fd);
        execlp("who", "who", NULL);

        perror("execlp who");
        exit(7);
    }
    else {
        int st;
        if (waitpid(gc, &st, 0) == -1) { 
            perror("waitpid gc");
            exit(8); 
        }

        if (kill(pid2, SIGUSR1) == -1) {
            perror("kill(F2, SIGUSR1)");
            exit(9);
        }
        exit(0);
    }
}

int main(int argc, char* argv[]) {
    pid_t pid1, pid2;

    sigset_t block, oldmask;
    sigemptyset(&block);
    sigaddset(&block, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &block, &oldmask) == -1) { 
        perror("sigprocmask"); 
        return 1; 
    }

    int f2_to_p[2];
    if (pipe(f2_to_p) == -1) { 
        perror("pipe"); 
        return 1; 
    }

    if((pid2 = fork()) == -1) {
        perror("Eroare la fork fiu 2");
        return 1;
    }

    if(pid2 == 0) {
        close(f2_to_p[0]);
        fiu2_stuff(f2_to_p[1], &oldmask);
    }

    if((pid1 = fork()) == -1) {
        perror("Eroare la fork fiu 1");
        return 1;
    }

    if(pid1 == 0) {
        close(f2_to_p[0]);
        close(f2_to_p[1]);
        fiu1_stuff(pid2);
    }

    close(f2_to_p[1]);

    char buff[4096];
    ssize_t r;
    while ((r = read(f2_to_p[0], buff, sizeof(buff))) > 0) {
        if (write(STDOUT_FILENO, buff, r) < 0) {
            perror("write parent");
            break;
        }
    }
    close(f2_to_p[0]);

    int wait_status;
    waitpid(pid1, &wait_status, 0);
    waitpid(pid2, &wait_status, 0);

    return 0;
}