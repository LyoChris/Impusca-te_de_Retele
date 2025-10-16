#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

// Simulați comanda: “cat prog.c | grep "include" > prog.c” folosind fifo-uri si dup-uri (si exec)

#define FIFO_T_F1 "/tmp/t_f1"
#define FIFO_F1_F2 "/tmp/f1_f2"

int main(int argc, char* argv[]) {
    pid_t fiu1, fiu2, fiu3;
    if(mkfifo(FIFO_T_F1, 0666) < 0 && errno != EEXIST) {
        perror("Eroare la mkfifo");
        return 1;
    }
    if(mkfifo(FIFO_F1_F2, 0666) < 0  && errno != EEXIST) {
        perror("Eroare la mkfifo");
        return 1;
    }

    if((fiu3 = fork()) < 0) {
        perror("Eroare la fork in parinte");
        return 3;
    }

    if(fiu3 == 0) {
        int fd_in, fd_out;
        if((fd_in = open(FIFO_F1_F2, O_RDONLY)) < 0) {
            perror("Eroare la fifo f1_f2");
            exit(1);
        }
        if((fd_out = open("prog.c", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
            perror("Eroare la fifo f1_f2");
            exit(1);
        }

        dup2(fd_in, STDIN_FILENO);
        dup2(fd_out, STDOUT_FILENO);
        close(fd_in);
        close(fd_out);

        execlp("cat", "cat",NULL);
        perror("Eroare la exec cat");
        exit(2);
    }

     if((fiu2 = fork()) < 0) {
        perror("Eroare la fork in parinte");
        return 3;
    }

    if(fiu2 == 0) {
        int fd_in, fd_out;
        if((fd_in = open(FIFO_T_F1, O_RDONLY)) < 0) {
            perror("Eroare la fifo f1_f2");
            exit(1);
        }
        if((fd_out = open(FIFO_F1_F2, O_WRONLY)) < 0) {
            perror("Eroare la fifo f1_f2");
            exit(1);
        }

        dup2(fd_in, STDIN_FILENO);
        dup2(fd_out, STDOUT_FILENO);
        close(fd_in);
        close(fd_out);
        int flags = fcntl(STDIN_FILENO, F_GETFD);
        if (flags == -1) { perror("fcntl getfd"); exit(1); }
        flags &= ~FD_CLOEXEC;
        if (fcntl(STDIN_FILENO, F_SETFD, flags) == -1) { perror("fcntl setfd"); exit(1); }


        execlp("grep", "grep", "include", NULL);
        perror("Eroare la exec grep");
        exit(2);
    }

    if((fiu1 = fork()) < 0) {
        perror("Eroare la fork in parinte");
        return 3;
    }

    if(fiu1 == 0) {
        int fd_out;

        if((fd_out = open(FIFO_T_F1, O_WRONLY)) < 0) {
            perror("Eroare la fifo f1_f2");
            exit(1);
        }

        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);

        execlp("cat", "cat", "prog.c", NULL);
        perror("Eroare la exec cat");
        exit(2);
    }

    int status;
    waitpid(fiu1, &status, 0);
    waitpid(fiu2, &status, 0);
    //waitpid(fiu3, &status, 0);

    unlink(FIFO_T_F1);
    unlink(FIFO_F1_F2);
    return 0;

}