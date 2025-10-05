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

typedef struct Opt {
    int arg_poz;
    bool b;
} Optiuni;



Optiuni analyse_argument(int argc, char* argv[]) {
    Optiuni a = {0, false};
    if (argc < 2) {
        fprintf(stderr,  "Usage: %s <options> <file name 1> ... \n", argv[0]);
        exit(1);
    }

    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--before") == 0) {
            a.b = true;
            a.arg_poz = i;
        }
    }
    
    return a;
}

void print_reversal(char* argv, Optiuni a) {
    int fd;
    struct stat sb;

    if((fd = open(argv, O_RDONLY)) == -1) {
        perror("Couldn't open file");
        exit(2);
    }

    if(stat(argv, &sb) == -1) {
        perror("Error at stat");
        exit(3);
    }

    char* map_in = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(map_in == MAP_FAILED) {
        perror("Eroare la map");
        exit(4);
    }

    if(close(fd) == -1) {
        perror("Error at close");
        exit(5);
    }

    char s[1024];
    int counter = 0;
    for(int i = sb.st_size - 1; i >= 0; i--) {
        if(map_in[i] != '\n' || i == 0) {
            s[counter++] = map_in[i];
        }
        else if (counter > 0) {
                for (int j = 0, k = counter - 1; j <= k; j++, k--){
                    char aux = s[k];
                    s[k] = s[j];
                    s[j] = aux;
                }
                s[counter]='\0';
                if(a.b == true) {
                    printf("\n%s", s);
                }
                else {
                    printf("%s\n", s);
                }
                counter = 0;
        }
    }
    for (int j = 0, k = counter - 1; j <= k; j++, k--){
                    char aux = s[k];
                    s[k] = s[j];
                    s[j] = aux;
                }
    s[counter]='\0';
    if(a.b == true) {
        printf("\n%s", s);
    }
    else {
        printf("%s\n", s);
    }
}

int main(int argc, char* argv[]) {
    Optiuni a = analyse_argument(argc, argv);
    int fd;
    struct stat sb;

    for(int i = 1; i < argc; i++) {
        if(i != a.arg_poz) {
            print_reversal(argv[i], a);
        }
    }

    return 0;
}
