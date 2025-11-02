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

#define PORT 4088

typedef struct {
    char username[64];
    bool in_use;
    int pid;
}User_sessions;

User_sessions sessions[10];

User_sessions* make_or_find_session(int id) {
    for(int i = 0; i < 10; i++) {
        if(sessions[i].pid == id) return &sessions[i];
    }
    for(int i = 0; i< 10; i++) {
        if(sessions[i].in_use == false) {
            sessions[i].pid = id;
            sessions[i].username[0] = '\0';
            sessions[i].in_use = true;

            return &sessions[i];
        }
    }
    return NULL;
}

void write_payload(char* payload, int fd) {
    int payload_size = strlen(payload);
    int payload_size_net = htonl(payload_size);

    if (write(fd, &payload_size_net, sizeof(payload_size_net)) <= 0) {
        perror("Eroare la write() de la client 2");
        return;
    }
    
    if (write(fd, payload, payload_size) <= 0) {
        perror("Eroare la write la client 1");
        return;
    }
}

int main(int argc, char* argv[]) {
    struct sockaddr_in server;
    struct sockaddr_in from;
    int sock;
    char* request;
    uint32_t request_size;

    printf("[SERVER] Pornim serverul pe portul %d...\n", PORT);

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Eroare la socket");
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

    struct pollfd poll_fds[10];
    poll_fds[0].fd = sock;
    poll_fds[0].events = POLLIN;
    int nfd = 1;

    printf("[SERVER] Așteptăm conexiuni de la clienți...\n");

    while(true) {
        if(poll(poll_fds, nfd, -1) < 0) {
            perror("Eroare poll");
            return 1;
        }

        for(int i = 0; i < nfd; i++) {
            if(!(poll_fds[i].events & POLLIN)) continue;

            if(poll_fds[i].fd == sock) {
                int client;
                if((client = accept(sock, NULL, NULL)) < 0) {
                    perror("Eroare la accept");
                    continue;
                }
                poll_fds[nfd].fd = client;
                poll_fds[nfd].events = POLLIN;
                nfd++;
            }
            else {
                memset(&request, 0, sizeof(request));

                if (read(poll_fds[i].fd, &request_size, sizeof(request_size)) <= 0) {
                    perror("Eroare la read() de la client 1");
                    close(poll_fds[i].fd);
                    poll_fds[i] = poll_fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }

                request_size = ntohl(request_size);
                request = malloc(request_size + 1);
                memset(request, 0, request_size + 1);

                if (read(poll_fds[i].fd, request, request_size) <= 0) {
                    perror("Eroare la read() de la client 2");
                    close(poll_fds[i].fd);
                    poll_fds[i] = poll_fds[nfd - 1];
                    nfd--;
                    i--;
                    continue;
                }

                char* command= malloc(request_size);
                int pid;
                sscanf(request, "%d|%s", &pid, command);

                printf("%s, %d\n", command, pid);
                User_sessions* s = make_or_find_session(pid);

                if(s == NULL) {
                    write_payload("Server busy, come again later", poll_fds[i].fd);
                    continue;
                }

                if(strncmp(request, "login", 5) == 0) {
                    
                }
            }
        }
    }
}