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
#include <signal.h> 
#include <dirent.h>
#include <time.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h> 
#include <utmp.h>


#define SERVER_COMM "/tmp/server_client_fifo"

typedef enum {LOG, HDR, FORW, EMPT} job_state;

typedef struct {
    char fifo[124];
    char user[64];
    bool in_use;
} User_session;

typedef struct {
    job_state st;
    int p_fd;
    int cli_fd;
    int sock_fd;
    char fifo_path[124];
    uint64_t len;
    pid_t child;
} Job;

void sig_handle(int sig) {
    unlink(SERVER_COMM);
    printf("\nSERVER s-a inchis\n");
    exit(0);
}

User_session sessions[256];
Job jobs[256];

int alloc() {
    for(int i = 0; i < 256; i++) {
        if(jobs[i].st == EMPT) return i;
    }
    return -1;
}

User_session* create_get_ses(char fifo[124]) {
    for(int i = 0; i < 256; i++) {
        if(strcmp(fifo, sessions[i].fifo) == 0 ) {
            if (sessions[i].in_use == true) return &sessions[i];
        }
    }
    for(int i = 0; i < 256; i++) {
        if (sessions[i].in_use == false) {
            strcpy(sessions[i].fifo, fifo);
            sessions[i].user[0] = '\0';
            sessions[i].in_use = true;
            return &sessions[i];
        }
    }
    return NULL;
}

User_session* find_ses(char fifo[124]) {
    for(int i = 0; i < 256; i++) {
        if(strcmp(sessions[i].fifo, fifo) == 0) return &sessions[i];
    }
    return NULL;
}

void job_reset(Job *j) {
    j->st = EMPT;
    if(j->p_fd >= 0) { close(j->p_fd); }
    if(j->cli_fd >= 0) { close(j->cli_fd); }
    if(j->sock_fd >= 0) { close(j->sock_fd); }
    j->child = -1;
    j->fifo_path[0] = '\0';
    j->len = 0;
}

void login_check(char user[64], int fd) {
    FILE* file;
    char msg[264];
    if(!(file = fopen("users.conf", "r"))) {
        perror("Error at opening users.conf");
        snprintf(msg, sizeof(msg), "ERR login %s", user);
        uint64_t len = strlen(msg);
        write(fd, &len, sizeof(len));
        write(fd, msg, len);
        return;
    }

    bool found = false;
    char line[256];
    while(fgets(line, sizeof(line), file)){
        if(line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
        if(strcmp(line,user)==0){ 
            found = true; 
            break; 
        }
    }

    if(found == true) {
        snprintf(msg, sizeof(msg), "OK login %s", user);
    }
    else {
        snprintf(msg, sizeof(msg), "ERR login %s", user);
    }

    uint64_t len = strlen(msg);
    write(fd, &len, sizeof(len));
    write(fd, &msg, len);

    fclose(file);
    close(fd);
    exit(0);
}

void get_logged_users(int fd) {
    setutent();
    char text[3280] = "";
    struct utmp *etr;

    while((etr = getutent()) != NULL) {
        if(etr->ut_type == USER_PROCESS) {
            char time[64];
            struct tm ti;
            time_t  t = etr->ut_tv.tv_sec;
            localtime_r(&t, &ti);
            strftime(time, sizeof(time), "%d--%m--%Y %H:%M:%S", &ti);

            char line[1024];
            snprintf(line, sizeof(line), "user: %s     host: %s    time_of_login: %s\n", etr->ut_user, etr->ut_host[0] ? etr->ut_host : "local", time);

            if (strlen(text) + strlen(line) < sizeof(text) - 1) strcat(text, line);
        } 
    }

    endutent();
    uint64_t len = strlen(text);
    write(fd, &len, sizeof(len));
    write(fd, &text, len);
    close(fd);
    exit(0);
}

void get_proc_info(int fd, char path[]) {
    FILE* file;
    if(!(file = fopen(path, "r"))) {
        perror("Eroare la fopen");
        char * error = "The specified process does not exist";
        uint64_t len = strlen(error);
        write(fd, &len, sizeof(error));
        write(fd, error, len);
        close(fd);
        exit(1);
    }

    char text[4000];
    char line[100];
    while(fgets(line, sizeof(line), file)) {
        if(strstr(line, "Name:") != NULL || strstr(line, "Uid:") != NULL || strstr(line, "State:") != NULL || strstr(line, "VmSize:") != NULL || strstr(line, "PPid:") != NULL) {
                printf("%s", line);
                strcat(text, line);
        }
    }

    printf("%s", text);

    uint64_t len = strlen(text);
    write(fd, &len, sizeof(len));
    write(fd, text, len);
    close(fd);
    fclose(file);
    exit(0);

}

void get_all_client_users(int fd) {
    char all_log_users[512];
    all_log_users[0] = '\0';
    strcat(all_log_users, "All the users currently logged-in are:\n");
    for(int i = 0; i < 256; i++) {
        if(sessions[i].in_use == true) {
            if(sessions[i].user[0] != '\0') {
                strcat(all_log_users, sessions[i].user);
                strcat(all_log_users, "\n");
            }
        }
    }
    if(all_log_users[strlen(all_log_users) - 1] == '\n') all_log_users[strlen(all_log_users) - 1] = '\0';

    uint64_t len = strlen(all_log_users);
    write(fd, &len, sizeof(len));
    write(fd, all_log_users, len);
    close(fd);
    exit(0);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, sig_handle);
    int fd_ser;
    printf("SERVER: Powered on\n");

    if (mkfifo(SERVER_COMM, 0666) == -1) { 
        if(errno == EEXIST) {}
        else {
            perror("mkfifo SERVER_COMM"); 
            return 1;
        } 
    }

    if ((fd_ser  = open(SERVER_COMM, O_RDWR)) < 0) { 
        perror("open SERVER_COMM in client"); 
        exit(2); 
    }

    printf("SERVER: opened SERVER_COMM\n");

    FILE* ser;
    ser = fdopen(fd_ser, "r+");
    setvbuf(ser, NULL, _IONBF, 0);

    for(int i = 0; i < 256; i++) job_reset(&jobs[i]);

    struct pollfd polld_fds[1 + 3 * 256];

    while(true) {
        int n = 0;
        polld_fds[n].fd = fd_ser;
        polld_fds[n].events = POLLIN;
        polld_fds[n++].revents=0;

        for(int i = 0; i < 256; i++) {
            if(jobs[i].st == LOG && jobs[i].sock_fd >= 0) {
                polld_fds[n].fd = jobs[i].sock_fd;
                polld_fds[n].events = POLLIN;
                polld_fds[n++].revents = 0; 
            }
            if((jobs[i].st == HDR || jobs[i].st == FORW) && jobs[i].p_fd >= 0) {
                polld_fds[n].fd = jobs[i].p_fd;
                polld_fds[n].events = POLLIN;
                polld_fds[n++].revents = 0; 
            }
        }

        int r, id = 0;
        if((r = poll(polld_fds, n, -1)) < 0) { 
            if(errno==EINTR) continue; 
            else {
                perror("Eroare poll");
                exit(2); 
            }
        }

        if(polld_fds[id].revents & POLLIN) {
            char fifo_path[124], comm[3972];
            if(fgets(fifo_path, sizeof(fifo_path), ser) && fgets(comm, sizeof(comm), ser)){
                if(fifo_path[strlen(fifo_path) -1] == '\n') fifo_path[strlen(fifo_path) - 1] = '\0';
                if(comm[strlen(comm) - 1] == '\n') comm[strlen(comm) - 1] = '\0';

                printf("%s\n", comm);
                printf("%s\n", fifo_path);

                User_session *s = create_get_ses(fifo_path);
                if(s == NULL) {
                    int cli_fd;
                    if((cli_fd =open(fifo_path,O_WRONLY|O_NONBLOCK)) < 0) {
                        perror("Eroare deschidere fifo in server la eroare server");
                        exit(2);
                    }
                    
                    char* msg = "Server busy. Try again later";
                    uint64_t len = strlen(msg);
                    write(cli_fd, &len, sizeof(len));
                    write(cli_fd, &msg, strlen(msg));
                }
                else if (strncmp(comm, "login", 5) == 0) {
                    char username[64] = {0};
                    sscanf(comm, " login : %63s", username);

                    if(s->user[0] != '\0') {
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Eroare deschidere fifo in server la eroare server");
                            exit(2);
                        }
                        char* msg = "Already authentificated. Log out first to be able to log in as another user.";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        int sock_fd[2];
                        if(socketpair(AF_UNIX,SOCK_STREAM,0,sock_fd) < 0) {
                            perror("Couldn't open sokcet pair connection with child for login");
                            exit(3);
                        }

                        int fl = fcntl(sock_fd[0], F_GETFL, 0); 
                        fcntl(sock_fd[0], F_SETFL, fl | O_NONBLOCK);

                        pid_t pid;
                        if((pid = fork()) < 0) {
                            perror("Error at creating son");
                            exit(4);
                        }

                        if(pid == 0) {
                            close(sock_fd[0]);
                            login_check(username, sock_fd[1]);
                        }

                        close(sock_fd[1]);
                        int j;
                        if((j = alloc()) >= 0) {
                            jobs[j].st = LOG;
                            jobs[j].sock_fd = sock_fd[0];
                            jobs[j].p_fd = jobs[j].cli_fd = -1;
                            jobs[j].child = pid;
                            strcpy(jobs[j].fifo_path, fifo_path);
                        }
                        else {
                            close(sock_fd[0]);
                        }
                    }
                }
                else if(strcmp(comm, "get-logged-users") == 0) {
                    if(s->user[0] == '\0') {
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "ERR: not authenticated";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        int pipe_fd[2];
                        if(pipe(pipe_fd) < 0) {
                            perror("Couldn't open pipe for get-logged-users");
                            exit(4);
                        }

                        int fl = fcntl(pipe_fd[0], F_GETFL, 0); 
                        fcntl(pipe_fd[0], F_SETFL, fl | O_NONBLOCK);

                        pid_t pid;
                        if((pid = fork()) < 0) {
                            perror("Error at creating son");
                            exit(4);
                        }

                        if(pid == 0) {
                            close(pipe_fd[0]);
                            get_logged_users(pipe_fd[1]);
                        }

                        close(pipe_fd[1]);
                        int j;
                        if((j = alloc()) >= 0) {
                            jobs[j].st = HDR;
                            jobs[j].sock_fd = -1;
                            jobs[j].p_fd = pipe_fd[0];
                            jobs[j].cli_fd = -1;
                            jobs[j].child = pid;
                            strcpy(jobs[j].fifo_path, fifo_path);
                        }
                        else {
                            close(pipe_fd[0]);
                        }
                    }
                }
                else if(strncmp(comm, "get-proc-info", strlen("get-proc-info")) == 0) {
                     if(s->user[0] == '\0') {
                        int cli_fd;
                        if((cli_fd =open(s->fifo, O_WRONLY)) < 0) {
                            if(errno == EEXIST) {}
                            else {
                                perror("Eroare deschidere fifo in server la eroare server");
                                exit(2);
                            }
                        }
                        char* msg = "ERR: not authenticated";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        char pid[64] = {0}; 
                        sscanf(comm, " get-proc-info : %63s", pid);
                        char path[120] = {0};
                        snprintf(path, sizeof(path), "/proc/%s/status", pid);
                        printf("%s\n", path);
                        int pipe_fd[2];
                        if(pipe(pipe_fd) < 0) {
                            perror("Couldn't open pipe for get-logged-users");
                            exit(4);
                        }

                        int fl = fcntl(pipe_fd[0], F_GETFL, 0); 
                        fcntl(pipe_fd[0], F_SETFL, fl | O_NONBLOCK);

                        pid_t pid1;
                        if((pid1 = fork()) < 0) {
                            perror("Error at creating son");
                            exit(4);
                        }

                        if(pid1 == 0) {
                            close(pipe_fd[0]);
                            get_proc_info(pipe_fd[1], path);
                        }

                        close(pipe_fd[1]);
                        int j;
                        if((j = alloc()) >= 0) {
                            jobs[j].st = HDR;
                            jobs[j].sock_fd = -1;
                            jobs[j].p_fd = pipe_fd[0];
                            jobs[j].cli_fd = -1;
                            jobs[j].child = pid1;
                            strcpy(jobs[j].fifo_path, fifo_path);
                        }
                        else {
                            close(pipe_fd[0]);
                        }
                    }
                }
                else if(strcmp(comm, "logout") == 0) {
                    if(s->user[0] == '\0') {
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "Can't log out if not already logged in.";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        s->user[0] = '\0';
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "Logout Succesfull";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                }
                else if(strcmp(comm, "quit") == 0) {
                    s->in_use = false;
                    int cli_fd;
                    if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                        perror("Error at opening FIFO in server");
                        exit(2);
                    }
                    char* msg = "Quitting succesfull. Closing connection to server.";
                    uint64_t len = strlen(msg);
                    write(cli_fd, &len, sizeof(len));
                    write(cli_fd, msg, strlen(msg));
                    close(cli_fd);
                }
                else if(strcmp(comm, "help") == 0) {
                     int cli_fd;
                    if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                        perror("Eroare deschidere fifo in server la eroare server");
                        exit(2);
                    }
                    char *msg = "The commands available are:\n"
                                "  login : <username> -- to login into the server\n"
                                "  get-logged-users -- shows info about all logged users (auth required)\n"
                                "  get-proc-info : <pid> -- shows info about a specific process (auth required)\n"
                                "  get-all-client-users -- shows usernames of all logged users (admin auth required)\n"
                                "  logout -- logs out the current user\n"
                                "  quit -- closes the client and disconnects from the server\n"
                                "  quit-server -- closes the server and all its connections (admin auth required)\n";
                    uint64_t len = strlen(msg);
                    write(cli_fd, &len, sizeof(len));
                    write(cli_fd, msg, strlen(msg));
                    close(cli_fd);
                }
                else if(strcmp(comm, "get-all-client-users") == 0) {
                    if(strcmp(s->user, "admin") != 0) {
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "ERR: not enough privileges";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        int pipe_fd[2];
                        if(pipe(pipe_fd) < 0) {
                            perror("Couldn't open pipe for get-users");
                            exit(4);
                        }

                        int fl = fcntl(pipe_fd[0], F_GETFL, 0); 
                        fcntl(pipe_fd[0], F_SETFL, fl | O_NONBLOCK);

                        pid_t pid;
                        if((pid = fork()) < 0) {
                            perror("Error at creating son");
                            exit(4);
                        }

                        if(pid == 0) {
                            close(pipe_fd[0]);
                            get_all_client_users(pipe_fd[1]);
                        }

                        close(pipe_fd[1]);
                        int j;
                        if((j = alloc()) >= 0) {
                            jobs[j].st = HDR;
                            jobs[j].sock_fd = -1;
                            jobs[j].p_fd = pipe_fd[0];
                            jobs[j].cli_fd = -1;
                            jobs[j].child = pid;
                            strcpy(jobs[j].fifo_path, fifo_path);
                        }
                        else {
                            close(pipe_fd[0]);
                        }
                    }
                }
                else if(strcmp(comm, "quit-server") == 0) {
                    if(strcmp(s->user, "admin") != 0) {
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "ERR: not enough privileges";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        for(int i = 0; i < 256; i++) job_reset(&jobs[i]);

                        unlink(SERVER_COMM);
                        printf("Server was clossed. Terminating all connections\n");
                        int cli_fd;
                        if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                        char* msg = "Server was clossed";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                        exit(0);
                    }
                }
                else {
                    int cli_fd;
                    if((cli_fd =open(fifo_path, O_WRONLY)) < 0) {
                        perror("Error at opening FIFO in server");
                        exit(2);
                    }
                    char* msg = "Wrong or inexistent command. Type 'help' to get a list of all available commands";
                    uint64_t len = strlen(msg);
                    write(cli_fd, &len, sizeof(len));
                    write(cli_fd, msg, strlen(msg));
                    close(cli_fd);
                }
            }
        }
        id += 1;
    
        for(int i = 0; i < 256; i++) {
            Job *j=&jobs[i];
            if(j->st==LOG && j->sock_fd >= 0 && id<n && polld_fds[id].fd==j->sock_fd){
                if(polld_fds[id].revents & POLLIN) {
                    uint64_t len;
                    read(j->sock_fd, &len, sizeof(len));
                    char* msg;
                    msg = malloc(len + 1); 
                    read(j->sock_fd, msg, len);
                    printf("%s\n", msg);

                    if(strncmp(msg, "OK", strlen("OK")) == 0) {
                        User_session *ses = find_ses(j->fifo_path);
                        char user[60] = {0};
                        sscanf(msg, "OK login %63s", user);
                        strcpy(ses->user, user);
                        int cli_fd;
                        if((cli_fd =open(j->fifo_path, O_WRONLY)) < 0) {
                            if(errno == EEXIST) {}
                            else {
                                perror("Error at opening FIFO in server");
                                exit(2);
                            }
                        }
                        char* msg = "OK: Authentification accepted. Welcome back";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    else {
                        int cli_fd;
                        if((cli_fd =open(j->fifo_path, O_WRONLY)) < 0) {
                            if(errno == EEXIST) {}
                            else {
                                perror("Error at opening FIFO in server");
                                exit(2);
                            }
                        }
                        char* msg = "ERR: Authentification rejected: no such username";
                        uint64_t len = strlen(msg);
                        write(cli_fd, &len, sizeof(len));
                        write(cli_fd, msg, strlen(msg));
                        close(cli_fd);
                    }
                    close(j->sock_fd);
                    job_reset(j);
                }
                id += 1;
            }
        }

        for(int i = 0; i < 256; i++) {
            Job *j=&jobs[i];
            if(j->st==HDR && j->p_fd >= 0 && id<n && polld_fds[id].fd==j->p_fd){
                if(polld_fds[id].revents & POLLIN) {
                    read(j->p_fd, &j->len, sizeof(j->len));
                    printf("SERVER: message sent length: %ld\n", j->len);
                    if((j->cli_fd = open(j->fifo_path, O_WRONLY)) < 0) {
                        if(errno == EEXIST) {}
                        else {
                            perror("Error at opening FIFO in server");
                            exit(2);
                        }
                    }
                    write(j->cli_fd, &j->len, sizeof(j->len));

                    j->st = FORW;
                }
            }
        }

        for(int i = 0; i < 256; i++) {
            Job *j=&jobs[i];
            if(j->st==FORW && j->p_fd >= 0 && id<n && polld_fds[id].fd==j->p_fd){
                if(polld_fds[id].revents & POLLIN) {
                    char* msg;
                    msg = malloc(j->len);
                    read(j->p_fd, msg, j->len);

                    printf("Messge sent to %s is:\n%s", j->fifo_path, msg);

                    write(j->cli_fd, msg, j->len);
                    close(j->cli_fd);
                    close(j->p_fd);
                    job_reset(j);
                }
            }
        }
    }
}