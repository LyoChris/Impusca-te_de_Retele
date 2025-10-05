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
#ifndef FNM_CASEFOLD
#define FNM_CASEFOLD 0x10
#endif


typedef struct Optiune {
    char* start;
    char* pattern;
    int L; //1 if follow symbolic links, something else if normal behaviour 
    int min_depth; // -1 infinit
    int max_depth; // -1 infinit
    int case_inse; //0 daca nu conteaza, 1 daca conteaza; 0 default
    long long int size_check; //0 daca nu este, valoarea verificata daca da
    int acc_check; // -1 daca nu este
    int mod_check; // -1 daca nu este
    char type_search;
    char size_check_type;
    char mod_check_type;
    char acc_check_type;
} Optiuni;

Optiuni process_options(int argc, char* argv[]) {
    Optiuni a = {0};
    a.min_depth = -1;
    a.max_depth = -1;
    a.acc_check = -1;
    a.mod_check = -1;

    for(int i = 1; i < argc; i++) {
        if(argv[i][0] == '-') {
            if(strcmp(argv[i], "-mindepth") == 0){
                a.min_depth = atoi(argv[++i]); 
                continue;
            }
            if(strcmp(argv[i], "-maxdepth") == 0) {
                a.max_depth = atoi(argv[++i]);
                continue;
            }
            if(strcmp(argv[i], "-name") == 0) {
                a.pattern = strdup(argv[++i]);
                continue;
            }
            if(strcmp(argv[i], "-iname") == 0) {
                a.pattern = strdup(argv[++i]);
                a.case_inse = 1;
                continue;
            }
            if(strcmp(argv[i], "-size") == 0) {
                int s = 0, k = 0;
                char prefix = 0;
                ++i;
                if (argv[i][0] == '+' || argv[i][0] == '-') {
                    a.size_check_type = argv[i][0];
                    k = 1;
                }
                for(k; k < strlen(argv[i]); k++) {
                    if(argv[i][k] >= '0' && argv[i][k] <= '9') {
                        s = s* 10 + (argv[i][k] - '0');
                    }
                    else {
                        prefix = argv[i][k];
                        break;
                    }
                }
                switch (prefix) {
                    case 0:  a.size_check = s * 512; break;
                    case 'b': a.size_check = s * 512; break;
                    case 'c': a.size_check = s; break;
                    case 'w': a.size_check = s * 2; break;
                    case 'k': a.size_check = s * 1024; break;
                    case 'M': a.size_check = s * 1024 * 1024; break;
                    case 'G': a.size_check = s * 1024 * 1024 * 1024; break;
                    default:
                        fprintf(stderr, "Unknown size suffix '%c'\n", prefix);
                        exit(2); 
                }
                continue;
            }
            if(strcmp(argv[i], "-type") == 0) {
                a.type_search = argv[++i][0];
                continue;
            }
            if(strcmp(argv[i], "-mtime") == 0) {
                int s = 0, k = 0;
                ++i;
                if (argv[i][0] == '+' || argv[i][0] == '-') {
                    a.mod_check_type = argv[i][0];
                    k = 1;
                }
                for(k; k < strlen(argv[i]); k++) {
                    if(argv[i][k] >= '0' && argv[i][k] <= '9') {
                        s = s* 10 + (argv[i][k] - '0');
                    }
                }
                a.mod_check = s;
                continue;
            }
            if(strcmp(argv[i], "-atime") == 0) {
                int s = 0, k = 0;
                ++i;
                if (argv[i][0] == '+' || argv[i][0] == '-') {
                    a.acc_check_type = argv[i][0];
                    k = 1;
                }
                for(k; k < strlen(argv[i]); k++) {
                    if(argv[i][k] >= '0' && argv[i][k] <= '9') {
                        s = s* 10 + (argv[i][k] - '0');
                    }
                }
                a.acc_check = s;
                continue;
            }
            if(strcmp(argv[i], "-L") == 0) {
                a.L = 1;
            }
        }
        else {
            a.start = strdup(argv[i]);
        }
    }

    if(a.start == NULL) {
        a.start = strdup(".");
    }

    return a;
}

bool verify_file(const char* file, const char* name, Optiuni a) {
    struct stat sb;
    int flags = 0;

    if(a.L == 1) {
        if(stat(file, &sb) != 0) {
            perror("Eroare la stat");
            return false;
        }
    }
    else {
        if(lstat(file, &sb) != 0) {
            perror("Eroare la stat");
            return false;
        }
    }

    if(a.case_inse) flags |= FNM_CASEFOLD;
    if((a.pattern != NULL) && fnmatch(a.pattern, name, flags) != 0) return false;
    if (a.size_check != 0) {
        switch (a.size_check_type) {
            case '+': if (sb.st_size <= a.size_check) return false; break;
            case '-': if (sb.st_size >= a.size_check) return false; break;
            default: if (sb.st_size != a.size_check) return false; break;
        }
    }
    if(a.mod_check != -1) {
        time_t now = time(NULL);
        double sec = difftime(now, sb.st_mtime);
        long days = (long)(sec / (60*60*24));
        switch (a.mod_check_type) {
            case '+': if(days <= a.mod_check) return false;
            case '-': if(days >= a.mod_check) return false;
            default:  if(days != a.mod_check) return false;
        }
    }
    if(a.acc_check != -1) {
        time_t now = time(NULL);
        double sec = difftime(now, sb.st_atime);
        long days = (long)(sec / (60*60*24));
        switch (a.mod_check_type) {
            case '+': if(days <= a.acc_check) return false;
            case '-': if(days >= a.acc_check) return false;
            default:  if(days != a.acc_check) return false;
        }
    }
    if(a.type_search != 0) {
        switch (a.type_search) {
            case 'f': if (!S_ISREG(sb.st_mode))  return false; break;
            case 'd': if (!S_ISDIR(sb.st_mode))  return false; break;
            case 'l': if (!S_ISLNK(sb.st_mode))  return false; break;
            case 'b': if (!S_ISBLK(sb.st_mode))  return false; break;
            case 'c': if (!S_ISCHR(sb.st_mode))  return false; break;
            case 'p': if (!S_ISFIFO(sb.st_mode)) return false; break;
            case 's': if (!S_ISSOCK(sb.st_mode)) return false; break;
            default:
                fprintf(stderr, "Unknown -type option: %c\n", a.type_search);
                return false;
        }
    }

    return true;
}

void file_walker(char* cale_cur, int level, Optiuni a) {
    DIR* dir = opendir(cale_cur);
    if (dir == NULL) {
        perror("Eroare parcurgere fisiere!");
        exit (1);
    }

    struct dirent* entry;
    struct stat info;

    while((entry = readdir(dir)) != NULL) {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        char cale_completa[PATH_MAX];
        sprintf(cale_completa,"%s/%s", cale_cur, entry->d_name);

        if(a.min_depth > 0 && level < a.min_depth) {}
        else {
            if(a.max_depth > 0 && level > a.max_depth) { return; }
            else {
                if(verify_file(cale_completa, entry->d_name, a) == true) {
                    //printf("Level is %d ;;", level);
                    printf("%s/%s\n", cale_cur, entry->d_name);
                }
            }
        }

        if(stat(cale_completa, &info) == 0 && S_ISDIR(info.st_mode)){
            file_walker(cale_completa, level + 1, a);
        }

    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    Optiuni a = process_options(argc, argv);
    file_walker(argv[1], 0, a);
}