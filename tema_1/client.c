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

#define SERVER_COMM "/tmp/server_client_fifo"
#define CLIENT_COMM "/tmp/client__response"

void connect_to_server(int* ser) {
    printf("CLIENT: Waiting to connect to server\n");
    while((*ser = open(SERVER_COMM, O_WRONLY)) == -1) {
        if(errno == ENOENT || errno == EEXIST) {}
        else {
            perror("Eroare la open server fifo in client");
            exit(2);
        }
    }
    printf("CLIENT: Connected to server\n");
}

int main(int argc, char* argv[]) {
    int fd_ser, fd_cli;
    char username[64];
    username[0] = '\0';
    if(argc > 1) {
        printf("Too many arguments when starting client.\n");
        exit(1);
    }

    char cli_fifo[28];
    snprintf(cli_fifo, sizeof(cli_fifo), "/tmp/client_%d_response", getpid());
    if(mkfifo(cli_fifo, 0666) < 0) {
        if(errno == EEXIST) {}
        else {
            perror("Error at creaing FIFO");
            exit(1);
        }
    }

    connect_to_server(&fd_ser);

    char* comm_line;
    size_t max_size = 0;
    ssize_t n = 0;
    while(true) {
        if(username[0] == '\0') {
            printf("USER: ");
            fflush(stdout);    
        }
        else {
            printf("%s: ", username);
            fflush(stdout);
        }

        if((n = getline(&comm_line, &max_size, stdin)) == -1) {
            exit(3); 
        }

        if(strlen(comm_line) == 1 && comm_line[0] == '\n') {continue; }
        if (comm_line[0] == '\x1b') { continue; }
        
        char message[strlen(comm_line) + 30];
        snprintf(message, sizeof(message), "%s\n%s", cli_fifo, comm_line);

        if(write(fd_ser, message, strlen(message)) < 0) {
                printf("SERVER quit unexpectedly. Closing connection and quiting client\n");
                close(fd_ser);
                unlink(cli_fifo);
                exit(4);
        }

        fd_cli = open(cli_fifo, O_RDONLY);
        if (fd_cli < 0) { perror("open client fifo"); continue; }

        uint64_t len = 0;
        if (read(fd_cli, &len, sizeof(len)) != sizeof(len)) {
            perror("read length");
            close(fd_cli);
            continue;
        }

        char payload[len + 1];
        ssize_t r2;
        if ((r2 = read(fd_cli, payload, len)) < 0) {
            perror("read payload");
            close(fd_cli);
            continue; 
        }
        payload[len] = '\0';


        printf("%s\n", payload);

        if(strncmp(comm_line, "quit-server", 5) == 0 && strstr(payload, "ERR") == NULL){
            printf("Now closing client\n");
            close(fd_ser);
            close(fd_cli);
            unlink(cli_fifo);
            exit(0);
        }

        if(strstr(payload, "Quit") != NULL && strcmp(comm_line, "quit\n") == 0){
            close(fd_ser);
            close(fd_cli);
            unlink(cli_fifo);
            exit(0);
        }

        if(strstr(payload, "OK: Authentification") != NULL){
            char user[64] = {0};
            sscanf(comm_line, "login : %63s", user);
            if(user[strlen(user) - 1] == '\n') user[strlen(user) - 1] = '\0';
            strcpy(username, user);
        }

        if(strstr(comm_line, "logout") != NULL){
            if(comm_line[strlen(comm_line) - 1] == '\n') comm_line[strlen(comm_line) - 1] = '\0';
            username[0] = '\0';
        }

        close(fd_cli);
    }
}
