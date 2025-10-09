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

// Execuția comenzii “ls -a -l” folosind execlp
// Cerinta I: Procesul tată creează un fiu. Procesul fiu apelează funcția execlp pentru a executa comanda. 
// Procesul tată așteaptă terminarea fiului
// După finalizarea fiului, tatăl afișează un mesaj de confirmare
// Cerinta II: Aceeași problemă utilizând execl
// Cerinta III: Implementarea utilizand execvp 
// Cerinta IV: Implementarea utilizand execv


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
        // execlp("ls","ls", "-a", "-l", NULL); //Cerinta I;

        // execl("/bin/ls", "ls", "-a", "-l", NULL); // Cerinta II;

        // char* args[] = {"ls", "-a", "-l", NULL}; // Cerinta III;
        // execvp("ls", args);

        char* args[] = {"ls", "-a", "-l", NULL}; // Cerinta IV'
        execv("/bin/ls", args);

        perror("Eroare la exec");
        exit(3);
    }

    int wait_code;

    wait(&wait_code);
    if(WIFEXITED(wait_code)) {
        printf("Child executed with code %d.\n", WEXITSTATUS(wait_code));
    }

    return 0;
}