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

// Scrieți un program care folosește două pipe-uri anonime pentru un dialog părinte↔fiu:
// Părintele trimite printr-un pipe un număr întreg.
// Copilul citește numărul, verifică dacă este prim și trimite răspunsul ("yes" / "no") printr-un alt pipe
// Părintele afișează răspunsul primit.

bool is_prime(int x) {
    for(int i = 2; i < x; i++) {
        if(x%i == 0) return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    int pip1[2], pip2[2];
    if(argc > 2 || argc <= 1) {
        printf("USAGE: %s <number>\n", argv[0]);
        return 1;
    }

    pid_t fiu;
    int numb = atoi(argv[1]);

    if(pipe(pip1) < 0) {
        perror("Eroare la pipe1");
        return 2;
    }

    if(pipe(pip2) < 0) {
        perror("Eroare la pipe2");
        return 2;
    }

    if((fiu = fork()) < 0) {
        perror("Eroare la fork");
        return 3;
    }

    if(fiu == 0) {
        close(pip1[1]);
        close(pip2[0]);

        int num;
        if((read(pip1[0], &num, sizeof(num))) < 0) {
            perror("Error at read");
            exit(1);
        }

        if(is_prime(num)) {
            write(pip2[1], "yes", strlen("yes") + 1);
        }
        else {
            write(pip2[1], "no", strlen("no") + 1);
        }
        close(pip1[0]);
        close(pip2[1]);
        exit(0);
    }
    else {
        close(pip1[0]);
        close(pip2[1]);

        char text[5];
        write(pip1[1], &numb, sizeof(numb));
        close(pip1[1]);

        read(pip2[0], text, sizeof(text));
        close(pip2[0]);

        printf("%s\n", text);
    }
}