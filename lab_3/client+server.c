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
#include <pwd.h>
#include <time.h>
#include <sys/socket.h>

// 5. Implementați un protocol de comunicare între două procese (părinte și fiu) folosind socketpair(AF_UNIX, SOCK_STREAM), în care:
// Părintele citește comenzi de la tastatură (stdin), câte una pe linie (terminate cu \n), și le trimite fiului.
// Fiul execută comanda și întoarce răspunsul ca șir de octeți prefixat cu lungimea pe 4 octeți (lungimea sa fie un int).
// Setul minim de comenzi suportate:
// login <username> — validează existența utilizatorului (de ex. verificând în /etc/passwd sau prin API de sistem echivalent);
// cd <cale> — schimbă directorul curent al procesului fiu;
// ls — listează conținutul directorului curent (formatul listării este la alegere, dar trebuie returnat ca payload);
// quit — închide sesiunea (fiul răspunde și apoi termină; părintele închide curat).
// Toate răspunsurile respectă formatul length-prefixed (inclusiv erorile).

void write_from_kid(int fd, char msg[]) {
    int len = strlen(msg) + 1;
    write(fd, &len, sizeof(len));
    write(fd, msg, len);
}

bool login(const char *uname) {
    struct passwd pw, *res = NULL;
    char buf[4096];
    int rc = getpwnam_r(uname, &pw, buf, sizeof(buf), &res);
    return rc == 0 && res != NULL;
}

void child_stuff(int fd) {
    bool log = false;
    char request[268];
    FILE* file;

    if((file = fdopen(fd, "r+")) == NULL) {
        perror("Eroare la fdopen child");
        char* msg = "ERR: Couldn't fdopen";
        write_from_kid(fd, msg);
        close(fd);
        exit(1);
    }

    while(true) {
        char payload[1024];
        payload[0] = '\0';
        if(!fgets(request, sizeof(request), file)) {
            perror("Eroare la fgets in child");
            char* msg = "ERR: Couldn't fgets";
            write_from_kid(fd, msg);
            close(fd);
            exit(1);
        }

        if(request[strlen(request) - 1] == '\n' ) request[strlen(request) - 1] = '\0';

        if(strncmp(request, "login", 5) == 0) {
            char name[64];

            if(sscanf(request, "login %s", name) == 1 && name[0]) {
                log = login(name);
                if(log == true) {
                    strcpy(payload, "Login accepted");
                }
                else {
                    strcpy(payload, "Login rejected: no such username");
                }
            }
            else {
                strcpy(payload, "ERR: usage: login <username>");
            }
        }
        else if(strncmp(request, "cd ", 3) == 0) {
            char dir[128];
            sscanf(request, "cd %s", dir);

            if(chdir(dir) < 0) {
                char* msg = "ERR: Couldn't chdir";
                write_from_kid(fd, msg);
                close(fd);
                exit(1);
            }
            else {
                strcpy(payload, "Changed directory to ");
                char dir1[128];
                getcwd(dir1, sizeof(dir1));
                strcat(payload, dir1);
            }
        }
        else if(strcmp(request, "ls") == 0) {
            pid_t son;
            int pip[2];

            if(pipe(pip) < 0) {
                perror("Eroare deschidere pipe");
                char* msg = "ERR: Couldn't pipe";
                write_from_kid(fd, msg);
                continue;
            }

            if((son = fork()) < 0) {
                perror("Eroare creare fiu");
                char* msg = "ERR: Couldn't fork in fiu";
                write_from_kid(fd, msg);
                continue;
            }

            if(son == 0) {
                close(pip[0]);
                dup2(pip[1], STDIN_FILENO);
                close(pip[1]);

                execlp("ls", "ls", NULL);
                perror("Eroare la execlp");
                char* msg = "ERR: Couldn't ls in fiu";
                write_from_kid(STDIN_FILENO, msg);
                exit(2);
            }
            else {
                char chunk[512];
                ssize_t rea;
                close(pip[1]);

                while((rea = read(pip[0], chunk, sizeof(chunk))) > 0) {
                    strcat(payload, chunk);
                }

                payload[strlen(payload)] = '\0';
                close(pip[0]);
            }
        }
        else if(strcmp(request, "quit") == 0) {
            char* msg = "Sever quited. Closing connection";
            write_from_kid(fd, msg);
            close(fd);
            exit(0);
        }
        else {
            char* msg = "Wrong or inexistent command";
            write_from_kid(fd, msg);
        }

        write_from_kid(fd, payload);
    }


    close(fd);
    exit(0);
}

int main(int argc, char* argv[]) {
    char message[268];
    pid_t fiu;
    int sock_fd[2];

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, sock_fd) < 0) {
        perror("Eroare deschidere socket");
        return 1;
    }

    if((fiu = fork()) < 0) {
        perror("Eroare creare fiu");
        return 2;
    }

    if(fiu == 0) {
        close(sock_fd[0]);
        child_stuff(sock_fd[1]);
        exit(0);
    }
    else {
        close(sock_fd[1]);
        while(true) {
            printf("Enter command: ");
            fflush(stdout);

            if(fgets(message, sizeof(message) - 1, stdin) == NULL) {
                perror("Eroare la citire comanda");
                continue;
            }

            write(sock_fd[0], message, strlen(message));

            int len = 0;
            char payload[512];
            if(read(sock_fd[0], &len, sizeof(len)) != sizeof(len)) {
                perror("Eroare la citire raspuns");
                continue;
            }

            read(sock_fd[0], payload, len);

            printf("%s\n", payload);

            if(strcmp(payload, "Sever quited. Closing connection") == 0) {
                close(sock_fd[0]);
                printf("Client quitted\n");
                return 0;
            }
        }
    }

    return 0;
}