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

#define PORT 4088

uint32_t id = 0;

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    uint32_t number;
    int sock;

    printf("[server] Pornim serverul pe portul %d...\n", PORT);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket");
        return 1;
    }
    printf("[server] Socket creat cu succes (fd=%d)\n", sock);

    memset(&server, 0, sizeof(server));
    memset(&from, 0, sizeof(from));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);
    printf("[server] Configurare adresa: INADDR_ANY:%d\n", PORT);

    if(bind(sock, (struct sockaddr*) &server, sizeof(server)) == -1) {
        perror("Eroare la bind");
        return 2;
    }
    printf("[server] Bind reușit.\n");

    if(listen(sock, 10) == -1) {
        perror("Eroare la listen");
        return 3;
    }
    printf("[server] Ascultam conexiuni (listen backlog=10)\n");
     
    while(true) {
        int client;
        socklen_t length = sizeof(from);

        printf("[server] Așteptăm conexiuni de la clienți...\n");
        if((client = accept(sock, (struct sockaddr*) &from, &length)) < 0) {
            perror("Eroare la accept");
            continue;
        }

        memset(&number, 0, sizeof(number));

        printf("[server] Așteptăm un număr de la client...\n");
        if (read(client, &number, sizeof(number)) <= 0) {
            perror("Eroare la read() de la client");
            close(client);
            continue;
        }

        uint32_t inc_number = htonl(ntohl(number) + 1);
        printf("[server] Incrementăm și trimitem numărul: %u\n", ntohl(number) + 1);

        uint32_t new_id = htonl(id + 1);
        printf("[server] ID client curent: %u\n", id + 1);

        if (write(client, &inc_number, sizeof(inc_number)) <= 0 ||
            write(client, &new_id, sizeof(new_id)) <= 0) {
            perror("[server] Eroare la write() către client");
            close(client);
            continue;
        } else {
            printf("[server] Răspuns trimis cu succes către client.\n");
        }

        printf("[server] Închidem conexiunea cu clientul %u\n", id + 1);
        close(client);
        id++;
    }
}
