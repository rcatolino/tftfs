#include <assert.h>
#include <errno.h>
#include <jansson.h>
#include <string.h>

#include "response_parsing.h"
#include "utils.h"

int get_path(const json_t *file, const char **path_res, const char *caller) {
  // Return -1 on error, 0 otherwise
  // on success path is garanteed to be a valid pointer
  // to data of types const char*;
  // It must not be freed directly. json_decref of the root will take care of it.
  json_t *path;

  assert(path_res);
  if (!json_is_object(file)) {
    fuse_debug("Error in %s, file is not an object\n", caller);
    return -1;
  }

  path = json_object_get(file, "path");
  if (!path || !json_is_string(path)) {
    fuse_debug("Error in %s, no path for this file\n", caller);
    return -1;
  } else {
    *path_res = json_string_value(path);
  }

  return 0;
}

int get_content(const json_t *file, const char **content_res, const char *caller) {
  // Return -1 on error, 0 otherwise
  // on success path is garanteed to be a valid pointer
  // to data of types const char*;
  // It must not be freed directly. json_decref of the root will take care of it.
  json_t *content;

  assert(content_res);
  if (!json_is_object(file)) {
    fuse_debug("Error in %s, file is not an object\n", caller);
    return -1;
  }

  content = json_object_get(file, "content");
  if (!content || !json_is_string(content)) {
    fuse_debug("Error in %s, no content for this file\n", caller);
    return -1;
  } else {
    *content_res = json_string_value(content);
  }

  return 0;
}

int get_metadata(const json_t *file, const char **type_res, time_t *mtime,
                 size_t *size, const char *caller) {
  // Return -1 on error, 0 otherwise
  // on success type, mtime and content are garanteed to be valid pointers
  // to data of respective types const char*, time_t and const char*
  // Either type or mtime can be null, in which case it won't be initialized.
  // In any case they must not be freed directly. json_decref of the root will take care of it.
  // mtime is returned in seconds, not milliseconds. (as expected for a time_t).
  json_t *meta;

  assert(type_res || mtime || size);
  if (!json_is_object(file)) {
    fuse_debug("Error in %s, file is not an object\n", caller);
    return -1;
  }

  meta = json_object_get(file, "meta");
  if (!meta || !json_is_object(meta)) {
    fuse_debug("Error in %s, no metadata in the file\n", caller);
    return -1;
  } else {
    // TODO: factorize the get into a macros
    if (type_res) {
      json_t *type = json_object_get(meta, "type");
      if (!type || !json_is_string(type)) {
        fuse_debug("Error in %s, no type for this file\n", caller);
        return -1;
      }

      *type_res = json_string_value(type);
    }

    if (mtime) {
      json_t *last_modified = json_object_get(meta, "Last-Modified");
      if (!last_modified || !json_is_integer(last_modified)) {
        fuse_debug("Error in %s, no mtime for this file\n", caller);
        return -1;
      }

      *mtime = json_integer_value(last_modified)/1000;
    }

    if (size) {
      json_t *content_length = json_object_get(meta, "Content-Length");
      if (!content_length || !json_is_integer(content_length)) {
        fuse_debug("Error in %s, no size for this file\n", caller);
        return -1;
      }

      *size = json_integer_value(content_length);
    }
  }

  return 0;
}

int load_buffer(char *buf, size_t size, json_t **root, const char *caller) {
  json_error_t err;
  json_t *tree_err;
  json_t *data_root = json_loadb(buf, size, 0, &err);
  *root = NULL;
  if (!data_root) {
    if (strncmp(err.text, "premature end of input", sizeof("premature end of input"))) {
      // We need to get more data. Problem : the data buffers are controlled by libcurl,
      // so we have to copy everything into our own huge buffer. Happily this shouldn't
      // happen to often. Let the caller handle that.
      fuse_debug("Incomplete chunk in %s\n", caller);
      return -2;
    }
    fuse_debug("Error in %s, %s at %d,%d :\n\t%s\n",
               caller, err.text, err.line, err.column, err.source);
    return -1;
  }

  // Test if the answer is an object :
  if (!json_is_object(data_root)) {
    fuse_debug("Error in %s, the received answer is not a valid json object\n", caller);
    goto err;
  }

  // Test if there is an "err" key at the root.
  tree_err = json_object_get(data_root, "err");
  if (tree_err) {
    int ret = 0;
    json_t *errno_code = json_object_get(tree_err, "errno");
    if (!errno_code || !json_is_integer(errno_code)) {
      fuse_debug("Error in %s, received an error from the server without any errno.\n", caller);
      goto err;
    }

    switch (json_integer_value(errno_code)) {
      // I'd like to be able to just send back the errno as is, but i can't garantee
      // that the values of each errno will be the same on the server and client
      // platforms. Actually we should check the error string, but well... I'm lazy.
      case 53:
        ret = ENOTEMPTY;
        break;
      case 34:
        ret = ENOENT;
        break;
      default:
        // this code should be : call the dev and tell him to add a new case!
        // ah wth this should work in most cases.
        ret = json_integer_value(errno_code);
        fuse_debug("Error code not translated! %d\n", ret);
        break;
    }

    json_decref(data_root);
    return ret;
  }

  *root = data_root;
  return 0;

err:
  json_decref(data_root);
  return -1;
}

int load_response(char *buf, size_t size, json_t **result, const char* caller) {
  // Returns -1 on format error,
  // 1 on server error,
  // 0 on success,
  // 2 if the chunk is incomplete.
  json_t *data_root;
  json_t *files;
  *result = NULL;

  int ret = load_buffer(buf, size, &data_root, caller);
  if (ret != 0) {
    return ret;
  }

  files = json_object_get(data_root, "files");
  if (!files || !json_is_array(files) || json_array_size(files) == 0) {
    fuse_debug("Error in %s, no files in the answer\n", caller);
    goto format_err;
  }

  json_incref(files);
  json_decref(data_root);
  *result = files;
  return 0;

format_err:
  json_decref(data_root);
  return -1;
}
