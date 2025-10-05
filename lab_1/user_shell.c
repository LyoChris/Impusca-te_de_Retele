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
#include <pwd.h>
#include <limits.h>
#include <time.h>

int main(int argc, char* argv[]) {
    if(argc != 1) {
        fprintf(stderr, "Incorrect arguments. USAGE: %s\n", argv[0]);
        return 2;
    }

    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);

    if(pw) {
        printf("The user running this program is %s and the shell that is used is %s\n.", pw->pw_name, pw->pw_shell);
    }
    else {
        perror("error at getting info about user");
        return 1;
    }

    return 0;
}