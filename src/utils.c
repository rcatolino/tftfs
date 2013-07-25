#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include "utils.h"

int isdir(char * path) {
  struct stat stats;
  if (stat(path, &stats) == -1) {
    perror("stat ");
    return 0;
  }

  if (S_ISDIR(stats.st_mode)) {
    return 1;
  } else {
    return 0;
  }
}
