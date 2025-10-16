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
#include <sys/socket.h>

// Părintele trimite un șir; fiul concatenează un sufix și trimite șirul rezultat înapoi (fork, comunicare socketpair)

int main(int argc, char* argv[]) {
    char* text = "child support";
    char rasp[110];
    pid_t fiu;
    int sock_fd[2];

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fd) < 0) {
        perror("Error at socket-pair");
        exit(1);
    }

    if((fiu = fork()) < 0) {
        perror("Eroare la fork");
        exit(2);
    }

    if(fiu == 0) {
        close(sock_fd[0]);
        char msg[100], payload[110];
        read(sock_fd[1], msg, sizeof(msg));
        strcpy(payload, "yes ");

        printf("%s\n", msg);
        strcat(payload, msg);

        write(sock_fd[1], payload, strlen(payload) + 1);

        close(sock_fd[1]);
        exit(0);
    }
    else {
        close(sock_fd[1]);
        write(sock_fd[0], text, strlen(text) + 1);

        read(sock_fd[0], rasp, sizeof(rasp));
        printf("%s\n", rasp);

        close(sock_fd[0]);
    }
    
    return 0;
}