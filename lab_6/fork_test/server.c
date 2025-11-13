#define _POSIX_C_SOURCE 200809L
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
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <netinet/in.h>
#include <signal.h>

#define PORT 4088

void handle_sig_child(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

void handle_client(int fd){
    while(true) {
        uint32_t net_size, size;
    char buffer[64];

    if (read(fd, &net_size, sizeof(net_size)) <= 0)
        return;
    size = ntohl(net_size);

    char *msg = malloc(size + 1);
    if (!msg) return;
    if (read(fd, msg, size) <= 0) {
        free(msg);
        return;
    }
    msg[size] = '\0';

    int number = atoi(msg);
    number++;

    snprintf(buffer, sizeof(buffer), "%d", number);
    uint32_t reply_size = htonl(strlen(buffer));

    write(fd, &reply_size, sizeof(reply_size));
    write(fd, buffer, strlen(buffer));

    free(msg);
    }
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sock;
    char* request;
    uint32_t request_size;

    //signal(SIGPIPE, SIG_IGN);

    printf("[SERVER] Pornim serverul pe portul %d...\n", PORT);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sig_child;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket");
        return 1;
    }

    int on = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    memset(&from, 0, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if(bind(sock, (struct sockaddr*) &server, sizeof(server)) == -1) {
        perror("Eroare la bind");
        return 2;
    }

     if(listen(sock, 10) == -1) {
        perror("Eroare la listen");
        return 3;
    }

    while(true) {
        int client;
        if((client = accept(sock, NULL, NULL)) < 0) {
            perror("Eroare la accept");
            continue;
        }

        pid_t pid;
        if((pid = fork()) < 0) {
            perror("Eroare la fork");
            close(client);
            continue;
        }

        if(pid == 0) {
            handle_client(client);
            //close(client);
            exit(0);
        }
        else {
            close(client);
        }
    }
    
    return 0;
}