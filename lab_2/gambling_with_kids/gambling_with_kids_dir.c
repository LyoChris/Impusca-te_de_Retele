#include <stdio.h>
#include <stdlib.h>
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

// Scrieți un program C în care un proces tată creează un proces fiu și verifică paritatea PID-ului fiului. În funcție de rezultat, tatăl transmite fiului unul dintre mesajele: 
//     "fortune" – dacă PID-ul fiului este par,
//     "lost" – dacă PID-ul fiului este impar (in acest caz va muri înaintea fiului)
// Implementați comunicarea între procese în trei variante distincte, conform situațiilor de mai jos:
//     S2. După crearea sa, fiul apelează o funcție, de exemplu monitDir(), care enumeră fișierele din directorul curent folosind struct dirent. Fiul verifică periodic conținutul directorului și detectează apariția unui fișier nou, presupunând că acesta conține mesajul de la tată. Fiul citește și afișează conținutul noului fișier.

bool is_in_list(char** list, int count, char* searched) {
    for(int i = 0; i < count; i++) {
        if(strcmp(list[i], searched) == 0) return true;
    }
    return false;
}

char** MonitDir(int *count) {
    DIR* dir = opendir(".");
    if (dir == NULL) {
        perror("Eroare parcurgere fisiere!");
        exit (1);
    }

    struct dirent *entry;
    char** list = NULL;
    *count = 0;

    while((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        list = realloc(list, (*count + 1) * sizeof(char *));
        list[*count] = strdup(entry->d_name);
        (*count)++;
    }

    closedir(dir);
    return list;
}

void child_stuff() {
    int count;
    char** prev_list = MonitDir(&count);

    while(true) {
        sleep(1);
        int new_count;
        char** list = MonitDir(&new_count);

        for(int i = 0; i < new_count; i++) {
            if(!is_in_list(prev_list, count, list[i])) {
                sleep(1);
                FILE* file = fopen(list[i], "r");
                char line [128];
                while (fgets(line, sizeof(line), file)) {
                    printf("%s\n", line);    
                }

                fclose(file);
                remove(list[i]);
                exit(0);
            }
        }

        for (int i = 0; i < count; i++) free(prev_list[i]);
        free(prev_list);
        count = new_count;
        prev_list = list;
    }

}

void parent_stuff(pid_t pid) {
    sleep(1);
    FILE* gambling = fopen("message.txt", "w");
    if (!gambling) {
        perror("fopen(parent)");
        exit(1);
    }
    printf("%d\n", pid);

    if(pid % 2 == 0) {
        fprintf(gambling, "treasure");
        fclose(gambling);
    }
    else {
        fprintf(gambling, "lost");
        fclose(gambling);
        exit(1);
    }
}

int main(int argc, char* argv[]) {
    if(argc > 1) {
        printf("Too many arguments.");
        exit(1);
    }

    pid_t pid;

    if((pid = fork()) == -1) {
        perror("Eroare la fork");
        exit(2);
    }

    if(pid == 0) {
        child_stuff();
        return 0;
    }

    parent_stuff(pid);

    wait(NULL);
    return 0;
}