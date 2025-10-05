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

// 2. Un program ce simuleaza grep.

typedef struct Optiune {
    char** pats;
    char** files;
    int f_count;
    int p_count;
    bool arld;
    short line_number; // equivalent to -n
    short case_inse; // equivalent to -i
    short invert_match; // equivalent to -v
    short count_match; //equivalent to -c
    short show_file; //1 for -H; 2 for -h; if two or more files present then -H default
    short invert_names; // equivalent to -l/-L(L - files witho match, l the reverse)
} Optiuni;

Optiuni process_args(int argc, char* argv[]) {
    Optiuni a = (Optiuni){0};
    a.arld =  false;

    a.pats  = malloc(10 * sizeof(char*));
    a.files = malloc((argc - 1) * sizeof(char*));

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            switch (argv[i][1]) {
                case 'n': a.line_number  = 1; break;
                case 'i': a.case_inse    = 1; break;
                case 'v': a.invert_match = 1; break;
                case 'c': a.count_match  = 1; break;
                case 'h': a.show_file    = 2; break;
                case 'H': a.show_file    = 1; break;
                case 'l': a.invert_names = 1; break;
                case 'L': a.invert_names = 2; break;

                case 'e': {
                    if (argv[i][2] != '\0') {
                        int same = 0;
                        for (int k = 0; k < a.p_count; k++) {
                            if (strcmp(argv[i] + 2, a.pats[k]) == 0) {
                                same = 1; break;
                            }
                        }
                        if (same != 1) {
                            a.pats[a.p_count] = strdup(argv[i] + 2);
                            a.p_count++;
                        }
                    }
                    else {
                            if (i + 1 >= argc) {
                                fprintf(stderr, "-e requires a pattern argument\n");
                                exit(2);
                            }
                            const char* pat = argv[i + 1];
                            int same = 0;
                            for (int k = 0; k < a.p_count; k++) {
                                if (strcmp(pat, a.pats[k]) == 0) { same = 1; break; }
                            }
                            if (!same) {
                                a.pats[a.p_count] = strdup(pat);
                                a.p_count++;
                            }
                            i++;
                    }
                    a.arld = true;
                    break;
                }
                case 'f': {
                    char* path;
                    if (argv[i][2] != '\0') {
                        path = strdup(argv[i] + 2);
                    } else {
                        if (i + 1 >= argc) {
                            fprintf(stderr, "-f requires a file argument\n");
                            exit(2);
                        }
                        path = strdup(argv[++i]);
                    }

                    FILE* file = fopen(path, "r");
                    if (!file) {
                        perror("fopen");
                        free(path);
                        exit(2);
                    }

                    char *line = NULL;
                    size_t len = 0;
                    ssize_t nread;

                    while ((nread = getline(&line, &len, file)) != -1) {
                        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
                            line[--nread] = '\0';
                        }
                        if (nread == 0) continue;

                        int same = 0;
                        for (int k = 0; k < a.p_count; k++) {
                            if (strcmp(line, a.pats[k]) == 0) {
                                same = 1; break;
                            }
                        }
                        if (same != 1) {
                            a.pats[a.p_count] = strdup(line);
                            a.p_count++;
                        }
                    }
                    free(line);
                    fclose(file);
                    free(path);
                    a.arld = true;
                    break;
                }
                default: {
                    fprintf(stderr, "Couldn't recognise option %s.\n", argv[i]);
                    break;
                }
            }
        } 
        else {
            a.files[a.f_count] = strdup(argv[i]);
            a.f_count++;
        }
    }

    if (a.arld != true) {
        if (a.f_count > 0) {
            int same = 0;
            for (int k = 0; k < a.p_count; k++) {
                if (strcmp(a.files[0], a.pats[k]) == 0) { same = 1; break; }
            }
            if (same != 1) {
                a.pats[a.p_count] = strdup(a.files[0]);
                a.p_count++;
            }
            for (int j = 1; j < a.f_count; j++) a.files[j - 1] = a.files[j];
            a.f_count--;
        } else {
            fprintf(stderr, "missing PATTERN\n");
            exit(2);
        }
    }

    if (a.show_file == 0 && a.f_count > 1) a.show_file = 1;

    return a;
}

void print_for_h(const char* txt) {
    printf("\033[35m%s\033[0m", txt);
}

void print_for_n(int txt) {
    printf("\033[32m%d\033[0m", txt);
}

int contains_inse(const char* s, const char* a) {
    if(strlen(a) == 0) return 1;
     for (const char *p = s; *p; p++) {
        size_t k = 0;
        while (k < strlen(a)) {
            char c1 = (char)tolower((unsigned char)p[k]);
            char c2 = (char)tolower((unsigned char)a[k]);
            if (c1 != c2) break;
            k++;
        }
        if (k == strlen(a)) return 1;
    }
    return 0;
}

int line_match(const char* line, Optiuni a) {
    for(int i = 0; i < a.p_count; i++) {
        if(a.case_inse == 1) {
            if(contains_inse(line, a.pats[i])) return 1;
        }
        else {
            if(strstr(line, a.pats[i]) != NULL) return 1;
        }
    }
    return 0;
}

void process_file(char* file, Optiuni a) {
    FILE* fp = fopen(file, "r");
    if(!fp) {
        perror("Failed at oppening file");
        return;
    }

    char* line = NULL;
    size_t len = 0;
    ssize_t nread;
    int line_count = 0, count_matches = 0;

    while ((nread = getline(&line, &len, fp)) != -1) {
        line_count++;
        
        int matched = line_match(line, a);

        if(a.invert_match == 1) {
            matched = !matched;
        }

        if (matched) {
            count_matches++;

            if(a.invert_names == 1) {
                print_for_h(file);
                printf("\n");
                break;
            }

            if(a.count_match == 0 && (a.invert_names == 1 || a.invert_names == 0)) {
                if (a.show_file == 1) {
                        if (a.line_number){
                            print_for_h(file);
                            printf(":");
                            print_for_n(line_count);
                            printf(":");
                        }
                        else{
                            print_for_h(file);
                            printf(":");
                        }
                } 
                else if (a.line_number) {
                        print_for_n(line_count);
                }      

                printf(" %s", line);
            }
        }

    }

    if (a.invert_names == 2) {
        if (count_matches == 0) {
            print_for_h(file);
            printf("\n");
        }
    }

    if (a.count_match) {
        if (a.show_file == 1 || (a.show_file == 0 && a.f_count > 1)) {
            print_for_h(file);
            printf(":%d\n", count_matches);
        }
        else
            printf("%d\n", count_matches);
    }

    free(line);
}

int main(int argc, char* argv[]) {
    Optiuni opt = process_args(argc, argv);

    for (int i = 0; i < opt.f_count; i++) {
        process_file(opt.files[i], opt);
    }
}
