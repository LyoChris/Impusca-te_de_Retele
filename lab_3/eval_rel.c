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
#include <sys/socket.h>

// // Scrieți un program în care procesul părinte primește o expresie aritmetică (de exemplu, prin linia de comandă sau stdin sau fisier) și o împarte 
// în mai multe sub-expresii independente. Pentru fiecare sub-expresie, părintele creează un proces fiu care o evaluează și îi trimite rezultatul părintelui. 
// Părintele combina rezultatele parțiale pentru a obține rezultatul final al expresiei și îl afișează (comunicarea socketpair)
