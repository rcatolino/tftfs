#ifndef RESP_PARSING_H__
#define RESP_PARSING_H__

int get_path(const json_t *file, const char **path_res, const char *caller);
int get_metadata(const json_t *file, const char **type_res, time_t *mtime, const char *caller);
int load_response(char *buf, size_t size, json_t ** files, const char * caller);

#endif
