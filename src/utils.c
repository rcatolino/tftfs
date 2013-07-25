#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>

#include "utils.h"

void print_help_head() {
  printf("Example: to mount http://thefiletree.com/example/ at ./tft use :\n"
         "\n"
         " tftfs http://thefiletree.com/example/ tft");
}

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
