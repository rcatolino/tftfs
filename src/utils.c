#include <assert.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "utils.h"

void print_help_head() {
  printf("Usage : tftfs [CURL OPTIONS] url mountpoint [FUSE_OPTIONS]\n"
         "Example: to mount http://thefiletree.com/example/ at ./tft use :\n"
         "\n"
         "tftfs http://thefiletree.com/example/ tft\n");
}

int isdir(const char * path) {
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

int end_option(const char * opt) {
  if (strcmp(opt, "--") == 0) {
    return 1;
  }

  return 0;
}

union curlparam get_option_param(const char * param_name, enum opttype type, GHashTable *params) {
  union curlparam param;
  long *ret;
  switch (type) {
    case STRING :
      param.strparam = param_name;
      break;
    case LONG :
      ret = g_hash_table_lookup(params, param_name);
      if (ret) {
        param.longparam = *ret;
      } else {
        param.longparam = -1;
      }
      break;
    case NONE :
      param.longparam = -1;
      break;
    default :
      debug("get_option_param : Unexpected option type.\n");
      assert(0);
  }

  return param;
}

int invalid_option(const char * opt, GHashTable *options) {
  // This function assumes that opt is valid
  const char * option_name = opt+2;
  CURLoption * co = g_hash_table_lookup(options, option_name);
  if (co) {
    debug("found %s\n", option_name);
    return 0;
  }

  debug("not found %s\n", option_name);
  return 1;
}

int option(const char * opt) {
  if (strncmp(opt, "--", 2) == 0) {
    return 1;
  }

  return 0;
}
