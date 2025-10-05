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
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <limits.h>
#include <time.h>

int main(int argc, char* argv[]) {
    if(argc == 1 || argc > 2) {
        fprintf(stderr, "Incorect form. USAGE: %s <path> \n", argv[0]);
        exit(1);
    }

    struct stat sb;
    if(lstat(argv[1], &sb) == -1) {
        perror("Error at stat");
        exit(2);
    }

    printf("The name of the file/directory is: %s an the last modification date is %s", basename(argv[1]), ctime(&sb.st_mtime));

}