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

#define PORT 4087

char* exec_key(char* key) {
    FILE* file;
    if(!(file = fopen("comm.conf", "r"))) {
        perror("Eroare la fopen");
        return "NU exista comenzi";
    }

    char line[100];
    while(fgets(line, sizeof(line), file)) {
        if(strncmp(line, key, strlen(key)) == 0) {
            char* c = strchr(line, ':');
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

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sock;
    char* key;
    uint32_t key_size;

     printf("[SERVER]: Pornim server...\n");

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("[CLIENT]: Eroare la sock");
        return 1;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(PORT);

    if(bind(sock, (struct sockaddr*) &server, sizeof(struct sockaddr)) < 0) {
        perror("[CLIENT]: Eroare la bind");
        return 2;
    }

    if(listen(sock, 10) == -1) {
        perror("Eroare la listen");
        return 3;
    }
    printf("[SERVER] Ascultam conexiuni (listen backlog=10)\n");

    struct pollfd fds[10];
    fds[0].fd = sock;
    fds[0].events = POLLIN;
    int nfd = 1;    

    printf("[SERVER] Așteptăm conexiuni de la clienți...\n");

    while(true) {
        if(poll(fds, nfd, -1) < 0) {
            perror("[SERVER]: Eroare la poll");
            return 4;
        }

        for(int i = 0; i < nfd; i++) {
            if(!(fds[i].revents & POLLIN)) continue;

            if(fds[i].fd == sock) {
                int client;
                memset(&from, 0, sizeof(from));
                socklen_t from_size = sizeof(from);
                if((client = accept(sock, (struct sockaddr*)&from, &from_size)) < 0) {
                    perror("Eroare la accept");
                    continue;
                }
                fds[nfd].fd = client;
                fds[nfd].events = POLLIN;
                nfd++;
            }
            else {
                memset(&key_size, 0, sizeof(key_size));

                if (read(fds[i].fd, &key_size, sizeof(key_size)) <= 0) {
                    perror("Eroare la read() de la client 1");
                    close(fds[i].fd);
                    fds[i] = fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }

                key_size = ntohl(key_size);
                key = malloc(key_size + 1);
                memset(key, 0, key_size + 1);

                if (read(fds[i].fd, key, key_size) <= 0) {
                    perror("Eroare la read() de la client 2");
                    //close(fds[i].fd);
                    fds[i] = fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }

                printf("Am primit: %s\n", key);

                char* raspuns = exec_key(key);
                printf("%s\n", raspuns);
                int rasp_size = strlen(raspuns);
                int rasp_size_net = htonl(rasp_size);

                if (write(fds[i].fd, &rasp_size_net, sizeof(rasp_size_net)) <= 0) {
                    perror("Eroare la write() de la client 2");
                    close(fds[i].fd);
                    fds[i] = fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }
                if (write(fds[i].fd, raspuns, rasp_size) <= 0) {
                    perror("Eroare la write la client 1");
                    close(fds[i].fd);
                    fds[i] = fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }
            }
        }
    }
}