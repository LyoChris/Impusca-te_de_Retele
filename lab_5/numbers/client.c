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
    int sock;
    struct sockaddr_in server;
    uint32_t number;

    printf("[client] Pornim clientul...\n");

    if (argc != 3) {
        printf ("USAGE: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    port = atoi(argv[2]);
    printf("[client] Adresa server: %s | Port: %u\n", argv[1], port);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[client] Eroare la socket");
        return 1;
    }
    printf("[client] Socket creat cu succes (fd=%d)\n", sock);

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(argv[1]);
    server.sin_port = htons(port);
    printf("[client] Structura sockaddr_in configurata.\n");

    printf("[client] Incercam conectarea la server...\n");
    if(connect(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("[client] Eroare la conectare server");
        return 2;
    }
    printf("[client] Conectat cu succes la server.\n");

    srand(time(NULL));
    number = htonl(rand() % 200);
    printf("[client] Trimitem numarul: %u\n", ntohl(number));

    if(write(sock, &number, sizeof(number)) <= 0) {
        perror("[client] Eroare la transmitere numar");
        close(sock);
        return 3;
    }
    printf("[client] Numar trimis catre server.\n");

    uint32_t second_number, unic_id;

    if(read(sock, &second_number, sizeof(number)) <= 0 || read(sock, &unic_id, sizeof(unic_id)) <= 0) {
        perror("[client] Eroare la primire raspuns");
        close(sock);
        return 3;
    }

    printf("[client] Am primit inapoi numarul: %u, clientul a avut id-ul: %u\n",
           ntohl(second_number), ntohl(unic_id));

    printf("[client] Inchidem conexiunea.\n");
    close(sock);

    return 0;
    
}
