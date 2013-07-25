#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

int main(int argc, char *argv[]) {

  int opt_readonly = 0;
  char **rest;
  GError *error = NULL;
  GOptionContext *opt_context;
  GOptionEntry opt_entries[] = {
    { "read-only", 'r', 0, G_OPTION_ARG_NONE, &opt_readonly, "Mount read-only", NULL},
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &rest,
      "The location of the tree, and the local mount point", "url mount-point"},
    { NULL }
  };

  opt_context = g_option_context_new("");
  g_option_context_set_summary(opt_context,
                               "Example: to mount http://thefiletree.com/example/ at ./tft use :\n"
                               "\n"
                               "  ./tftfs [-r] http://thefiletree.com/example/ tft");
  g_option_context_add_main_entries(opt_context, opt_entries, NULL);
  if (!g_option_context_parse(opt_context, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    goto parse_error;
  }

  if (opt_readonly) {
    debug("readonly\n");
    // Take care of readonly option
  }

  if (rest[0] == NULL || rest[1] == NULL) {
    printf("Error, you need to specify a tree url and a mount point on the local filesystem.%d\n",argc);
    goto arg_err;
  } else if (rest[2] != NULL) {
    printf("Bad argument : %s.\n", rest[2]);
    goto arg_err;
  }

  if (!isdir(rest[1])) {
    printf("Error, %s is not a valid directory\n", rest[1]);
    goto out;
  }

out :
  g_strfreev(rest);
arg_err :
  g_option_context_free(opt_context);
parse_error :
  if (error) {
    g_error_free(error);
    return 1;
  }

  return 0;
}
