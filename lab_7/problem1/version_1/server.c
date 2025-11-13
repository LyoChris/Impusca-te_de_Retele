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
#include <pthread.h>

#define PORT 4088

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

char* exec_key(char* key) {
    FILE* file;
    if(!(file = fopen("comm.conf", "r"))) {
        perror("Eroare la fopen");
        return "NU exista comenzi";
    }

    printf("Caut cheia %s in fisier...\n", key);

    char line[100];
    while(fgets(line, sizeof(line), file)) {
        char* c = strchr(line, ':');
        char name[(c-line) + 1];
        strncpy(name, line, c - line);
        name[c - line] = '\0';

        if(strcmp(name, key) == 0) {
            
            char* com = c + 1;
            pid_t pid;
            int sock[2];

            if(socketpair(AF_UNIX, SOCK_STREAM, 0, sock) < 0) {
                perror("Eroare la sockpair");
                return "Alta eroare";
            }

            if((pid = fork()) < 0) {
                perror("Eroare la fork");
                return "Alta eroare";
            }

            if(pid == 0) {
                close(sock[1]);
                dup2(sock[0], STDOUT_FILENO);
                close(sock[0]);
                system(com);
                exit(0);
            }
            else {
                close(sock[0]);
                char* rasp;
                rasp = malloc(3200);
                if(read(sock[1], rasp, 3200) < 0) {
                    perror("Eroare la read");
                    return "Alta eroare";
                }
                fclose(file);
                return rasp;
            }
        }
    }
    fclose(file);
    return "Unknown key";
}

void handle_client(int fd){
    char* key;
    uint32_t key_size;
    while(true) {
        memset(&key_size, 0, sizeof(key_size));

        if (read(fd, &key_size, sizeof(key_size)) <= 0) {
            perror("Eroare la read() de la client 1");
                continue;
            }

            key_size = ntohl(key_size);
            key = malloc(key_size + 1);
            memset(key, 0, key_size + 1);

            if (read(fd, key, key_size) <= 0) {
                    perror("Eroare la read() de la client 2");
                    continue;
                }

            printf("Am primit: %s\n", key);

            char* raspuns = exec_key(key);
            printf("%s\n", raspuns);
            int rasp_size = strlen(raspuns);
            int rasp_size_net = htonl(rasp_size);

            if (write(fd, &rasp_size_net, sizeof(rasp_size_net)) <= 0) {
                    perror("Eroare la write() de la client 2");
                    continue;
            }
            if (write(fd, raspuns, rasp_size) <= 0) {
                perror("Eroare la write la client 1");
                continue;
            }
    }
}

void *treat(void * arg) {
    struct thData tdL;
    tdL= *((struct thData*)arg);
    printf ("[thread]- %d - Asteptam mesajul...\n",tdL.idThread);
    fflush (stdout);	
    handle_client (tdL.cl);
    close (tdL.cl);
    printf("[thread]- %d - Am terminat cu clientul.\n",tdL.idThread);
    fflush (stdout);	
    return(NULL);	
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sock;
    char* request;
    uint32_t request_size;
    pthread_t th[100];
    int id = 0;

    printf("[SERVER] Pornim serverul pe portul %d...\n", PORT);

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
        thData* td;

        if((client = accept(sock, NULL, NULL)) < 0) {
            perror("Eroare la accept");
            continue;
        }

        td = (thData*)malloc(sizeof(thData));
        td->idThread = id++;
        td->cl = client;

        pthread_create(&th[id], NULL, &treat, td);
    }
    
    return 0;
}