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

uint16_t port;

int main(int argc, char* argv[]){
    struct sockaddr_in server;
    int sock;
    char* line;
    char* init_line;
    uint32_t line_size;
    size_t max_size = 0;
    ssize_t n = 0;

    printf("[client] Pornim clientul...\n");

    if (argc != 3) {
        printf ("USAGE: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[CLIENT]: Eroare la sock");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);

    printf("[CLIENT]: Incercam conectarea la server...\n");
    if(connect(sock, (struct sockaddr*) &server, sizeof(struct sockaddr)) < 0) {
        perror("[CLIENT]: Eroare la connect");
        return 2;
    }
    printf("[CLIENT]: Conectat cu succes la server...\n");

    while(true) {
        printf("[CLIENT]: Enter command: ");
        fflush(stdout);

        if((n = getline(&init_line, &max_size, stdin)) == -1) {
            return 3;
        }

        if(strlen(init_line) == 0) continue;

        line = malloc(strlen(init_line) + 8);
        snprintf(line, strlen(init_line) + 8, "%d|%s", getpid(), init_line);

        line[strlen(line) - 1] = '\0';
        line_size = htonl((uint32_t)strlen(line));

        if(write(sock, &line_size, sizeof(line_size)) <= 0 || write(sock, line, strlen(line)) <= 0) {
            perror("[client] Eroare la trimitere comanda");
            continue;
        }

        char* payload;
        uint32_t payload_size;

        if(read(sock, &payload_size, sizeof(payload_size)) <= 0) {
            perror("[client] Eroare la primire dimensiune raspuns");
            continue;
        }

        payload_size = ntohl(payload_size);
        payload = malloc(payload_size + 1);
        memset(payload, 0, payload_size + 1);
        if(read(sock, payload, payload_size) <= 0) {
            perror("[client] Eroare la primire raspuns");
            free(payload);
            continue;
        }

        printf("%s\n", payload);

        if(strcmp(payload, "exit") == 0) {
            printf("[CLIENT] Inchidem clientul...\n");
            free(payload);
            free(line);
            break;
        }
    }
}