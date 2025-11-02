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
#include <netinet/in.h>

uint16_t port;

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    uint32_t key;
    int sock;

    printf("[CLIENT]: Pornim clientul...\n");

    if(argc != 3) {
        printf("USAGE: %s <server_adress <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[CLIENT]: Eroare la sock");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    printf("[CLIENT]: Incercam conectarea la server...\n");
    if(connect(sock, (struct sockaddr*) &server, sizeof(struct sockaddr)) < 0) {
        perror("[CLIENT]: Eroare la connect");
        return 2;
    }
    printf("[CLIENT]: Conectat cu succes la server...\n");

    char* key_line;
    size_t max_size = 0;
    ssize_t n = 0;
    while(true) {
        printf("[CLIENT]: Scrie o cheie: ");
        fflush(stdout);

        if((n = getline(&key_line, &max_size, stdin)) == -1) {
            return 3;
        }
        key_line[strlen(key_line) - 1] = '\0';
        key = htonl((uint32_t)strlen(key_line));

        if(strcmp(key_line, "exit") == 0) {
            printf("[CLIENT]: Iesim din program...\n");
            close(sock);
            return 0;
        }

        if(write(sock, &key, sizeof(key)) <= 0 || write(sock, key_line, strlen(key_line)) <= 0) {
            perror("[client] Eroare la trimitere cheie");
            continue;
        }

        char* response;
        uint32_t response_size;

        if(read(sock, &response_size, sizeof(response_size)) <= 0) {
            perror("[client] Eroare la primire dimensiune raspuns");
            continue;
        }

        response_size = ntohl(response_size);
        response = malloc(response_size + 1);
        memset(response, 0, response_size + 1);
        if(read(sock, response, response_size) <= 0) {
            perror("[client] Eroare la primire raspuns");
            free(response);
            continue;
        }

        printf("[CLIENT]: Raspunsul este:\n %s\n", response);

    }
    
}