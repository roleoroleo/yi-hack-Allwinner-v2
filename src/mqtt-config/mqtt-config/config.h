#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

#define MAX_LINE_LENGTH     512
#define MAX_KEY_LENGTH      128
#define MAX_VALUE_LENGTH    128

void config_set_handler(void (*f)(const char* key, const char* value));
void config_parse(FILE *fp);
FILE *open_conf_file(const char* filename);
void close_conf_file(FILE *fp);
