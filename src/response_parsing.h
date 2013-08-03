#ifndef RESP_PARSING_H__
#define RESP_PARSING_H__

int get_path(const json_t *file, const char **path_res, const char *caller);
int get_content(const json_t *file, const char **content_res, const char *caller);
int get_metadata(const json_t *file, const char **type_res, time_t *mtime,
                 size_t *size, const char *caller);
int load_response(char *buf, size_t size, json_t ** files, const char *caller);
int load_buffer(char *buf, size_t size, json_t **root, const char *caller);

#endif
